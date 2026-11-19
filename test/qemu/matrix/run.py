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


def check_trace(lines, fault_pc, resume_pc, psp, attempts, recoveries):
    site_hits = 0
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
            site_hits += pc == fault_pc
            if after_return:
                returns.append((pc, None if magic is None else bool(magic & 4)))
                after_return = False
                magic = None

    wanted = [(resume_pc, psp)] * recoveries
    if after_return or returns != wanted or site_hits != attempts:
        raise RuntimeError(f"probe PC hits={site_hits}, actual returns={returns}, expected={wanted}")


def symbols(elf):
    output = subprocess.check_output(["arm-none-eabi-nm", "-n", elf], text=True)
    return {cols[2]: int(cols[0], 16) for line in output.splitlines()
            if len(cols := line.split()) == 3}


def limit_trace():
    resource.setrlimit(resource.RLIMIT_FSIZE, (32 << 20, 32 << 20))


def build(tmp, defines, sources):
    elf = tmp / "probe.elf"
    subprocess.run([*CC, *defines, "-DBUSFAULT_USE_VENDOR_STARTUP", "-nostdlib",
                    "-T", TEST / "link.ld", ROOT / "startup.S", ROOT / "boot_busfault.c",
                    TEST / "qemu_vectors.S", *sources, "-lgcc", "-o", elf], check=True)
    return elf


def run_qemu(elf, site, resume, psp, attempts, recoveries):
    trace = elf.with_suffix(".log")
    subprocess.run(["qemu-system-arm", "-M", "mps2-an505", "-cpu", "cortex-m33",
                    "-accel", "tcg,one-insn-per-tb=on", "-nographic", "-semihosting",
                    "-kernel", elf, "-d", "int,exec,nochain", "-D", trace],
                   check=True, timeout=15, preexec_fn=limit_trace)
    with trace.open() as lines:
        check_trace(lines, site, resume, psp, attempts, recoveries)


def main():
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        for width in (2, 4):
            print(f"RUN  original demo ({width} bytes)", flush=True)
            defs = ["-DDEMO_FAULT_ADDRESS=0x60000000U"]
            if width == 4:
                defs.append("-DDEMO_WIDE_LOAD")
            elf = build(tmp, defs, [ROOT / "examples/probe_read.S",
                                    ROOT / "examples/probe_read.c", TEST / "qemu_harness.c"])
            site = symbols(elf)["Demo_ProbeLoad"]
            # The first load succeeds. The second reaches the same PC and faults.
            run_qemu(elf, site, site + width, False, attempts=2, recoveries=1)
            print(f"PASS original demo ({width} bytes)", flush=True)

        for name, opcode, width, offset, flags in CASES:
            print(f"RUN  {name}: {opcode}", flush=True)
            defs = [f"-D{flag}" for flag in flags]
            defs += [f"-DPROBE_OP={opcode}", f"-DADDRESS_OFFSET={offset}"]
            elf = build(tmp, defs, [MATRIX / "access.S", MATRIX / "check.c"])
            labels = symbols(elf)
            site, resume = labels["Probe_Fault"], labels["Probe_Resume"]
            if resume - site != width:
                raise RuntimeError(f"{name}: assembler emitted {resume-site} bytes")
            fatal = "EXPECT_FATAL" in flags
            run_qemu(elf, site, resume, "USE_PSP" in flags,
                     attempts=1 if fatal else 2, recoveries=0 if fatal else 2)
            print(f"PASS {name}", flush=True)
    print(f"PASS: {len(CASES) + 2} QEMU cases")


if __name__ == "__main__":
    main()
