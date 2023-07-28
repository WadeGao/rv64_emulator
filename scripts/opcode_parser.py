import argparse


def ArgsBuild():
    parser = argparse.ArgumentParser(
        description="mask and data of instructions generator"
    )
    parser.add_argument(
        "--filename",
        type=str,
        required=True,
        help="offical opcode desc in github.com/riscv/riscv-opcodes",
    )
    return parser.parse_args()


fmt_pattern = "{0:^16}\t{1:^16}\t{2:^16}"


def parse(filename: str):
    with open(filename, "r") as f:
        for line in f.readlines():
            if line.startswith("#") or line in ["\n", "\r\n"] or line.strip() == "":
                continue
            inst_desc = line.split()

            opcode_fields = []
            for field in inst_desc[1:]:
                if ".." in field and "ignore" not in field:
                    opcode_fields.append(field)

            mask, data = 0, 0
            for field in opcode_fields:
                msb_end = field.find("..")
                lsb_start = msb_end + 2
                lsb_end = field.find("=")
                msb = int(field[:msb_end])
                lsb = int(field[lsb_start:lsb_end])
                val = int(field[lsb_end + 1:], 16)

                mask |= ((1 << (msb - lsb + 1)) - 1) << lsb
                data |= val << lsb

            inst_name = inst_desc[0]
            if inst_name == "$pseudo_op":
                inst_name = inst_desc[1]

            print(fmt_pattern.format(inst_name, hex(mask), hex(data)))


if __name__ == "__main__":
    args = ArgsBuild()
    print(fmt_pattern.format("name", "mask", "data"))
    parse(args.filename)
