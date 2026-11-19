"""Build an ELF per case and check actual exception-return PCs in QEMU's trace."""
from pathlib import Path
import re
import resource
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
TEST = ROOT / "test/qemu"
MATRIX = TEST / "matrix"
CC = ["arm-none-eabi-gcc", "-mcpu=cortex-m33+nodsp+nofp", "-mthumb",
      "-ffreestanding", "-Os", "-g", "-I", str(ROOT), "-I", str(TEST / "cmsis")]
# Name, instruction, width in bytes, data address offset, extra build flags.
CASES = [
    ("word-read", "ldr.n r1, [r0]", 2, 0, []),
    ("word-write", "str.n r1, [r0]", 2, 0, []),
    ("byte-read", "ldrb.n r1, [r0, #1]", 2, 1, []),
    ("byte-write", "strb.n r1, [r0, #1]", 2, 1, []),
    ("half-read", "ldrh.n r1, [r0, #2]", 2, 2, []),
    ("half-write", "strh.n r1, [r0, #2]", 2, 2, []),
    ("signed-byte", "ldrsb.n r1, [r0, r2]", 2, 0, []),
    ("signed-half", "ldrsh.n r1, [r0, r2]", 2, 0, []),
    ("wide-read", "ldr.w r1, [r0, #4]", 4, 4, []),
    ("wide-write", "str.w r1, [r0, #4]", 4, 4, []),
    ("wide-byte", "ldrb.w r1, [r0, #1]", 4, 1, []),
    ("wide-half", "strh.w r1, [r0, #2]", 4, 2, []),
    ("psp-read", "ldr.n r1, [r0]", 2, 0, ["USE_PSP"]),
    ("psp-wide-write", "str.w r1, [r0]", 4, 0, ["USE_PSP"]),
    ("outside", "ldr.n r1, [r0]", 2, 0, ["EXPECT_FATAL", "OUTSIDE_SECTION"]),
    ("outside-multiple", "ldmia r0!, {r1,r2}", 2, 0, ["EXPECT_FATAL", "OUTSIDE_SECTION"]),
    ("it-block", "ldreq.n r1, [r0]", 2, 0, ["EXPECT_FATAL", "IN_IT"]),
    ("disabled", "ldr.n r1, [r0]", 2, 0, ["EXPECT_FATAL", "ESCALATE"]),
]

TRACE_PC = re.compile(r"Trace \d+: .*?\[[0-9a-fA-F]+/([0-9a-fA-F]+)/")
MAGIC = re.compile(r"Exception return: magic PC ([0-9a-fA-F]+)", re.I)


def check_trace(lines, fault_pc, resume_pc, psp, fatal):
    faults = 0
    returns = []
    magic = None
    after_return = False
    for line in lines:
        match = MAGIC.search(line)
        if match:
            magic = int(match[1], 16)
        if "...successful exception return" in line:
            after_return = True
        match = TRACE_PC.search(line)
        if match:
            pc = int(match[1], 16)
            faults += pc == fault_pc
            if after_return:
                returns.append((pc, None if magic is None else bool(magic & 4)))
                after_return = False
                magic = None

    wanted = [] if fatal else [(resume_pc, psp)] * 2
    if after_return or returns != wanted or faults != (1 if fatal else 2):
        raise RuntimeError(f"fault PC hits={faults}, actual returns={returns}, expected={wanted}")


def symbols(elf):
    output = subprocess.check_output(["arm-none-eabi-nm", "-n", elf], text=True)
    return {cols[2]: int(cols[0], 16) for line in output.splitlines()
            if len(cols := line.split()) == 3}


def limit_trace():
    resource.setrlimit(resource.RLIMIT_FSIZE, (32 << 20, 32 << 20))


def main():
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        for name, opcode, width, offset, flags in CASES:
            print(f"RUN  {name}: {opcode}", flush=True)
            defs = [f"-D{flag}" for flag in flags]
            elf = tmp / "probe.elf"
            subprocess.run([*CC, *defs, "-DBUSFAULT_USE_VENDOR_STARTUP",
                            f"-DPROBE_OP={opcode}", f"-DADDRESS_OFFSET={offset}",
                            "-nostdlib", "-T", TEST / "link.ld", ROOT / "startup.S",
                            ROOT / "boot_busfault.c", TEST / "qemu_vectors.S",
                            MATRIX / "access.S", MATRIX / "check.c", "-lgcc", "-o", elf],
                           check=True)
            labels = symbols(elf)
            site, resume = labels["Probe_Fault"], labels["Probe_Resume"]
            if resume - site != width:
                raise RuntimeError(f"{name}: assembler emitted {resume-site} bytes")
            trace = tmp / "trace.log"
            subprocess.run(["qemu-system-arm", "-M", "mps2-an505", "-cpu", "cortex-m33",
                            "-accel", "tcg,one-insn-per-tb=on", "-nographic",
                            "-semihosting", "-kernel", elf, "-d", "int,exec,nochain",
                            "-D", trace], check=True, timeout=15, preexec_fn=limit_trace)
            with trace.open() as lines:
                check_trace(lines, site, resume, "USE_PSP" in flags, "EXPECT_FATAL" in flags)
            print(f"PASS {name}", flush=True)
    print(f"PASS: {len(CASES)} QEMU cases")


if __name__ == "__main__":
    main()
