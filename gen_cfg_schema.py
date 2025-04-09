import argparse
import sonic_yang

YANG_MODELS_DIR = "/usr/local/yang-models"
DEFAULT_OUTPUT_FILE = "common/cfg_schema.h"

def write_cfg_schema(keys, output_file="cfg_schema.h"):
    header = """#ifndef CFG_SCHEMA_H
#define CFG_SCHEMA_H

// Macros for table names are autogenerated by gen_cfg_schema.py. Manual update will not be preserved.
#ifdef __cplusplus
namespace swss {
#endif
"""
    footer = """
#ifdef __cplusplus
}
#endif
#endif"""

    with open(output_file, "w") as f:
        f.write(header)
        f.write("\n")
        for key in sorted(keys):
            if '-' in key:
                # Comment out keys with hyphens as they can't be valid C macros
                f.write('// #define CFG_{}_TABLE_NAME "{}"\n'.format(key, key))
            else:
                f.write('#define CFG_{}_TABLE_NAME "{}"\n'.format(key, key))
        f.write(footer)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d',
                        dest='dir',
                        metavar='yang directory',
                        type=str,
                        help='the yang directory to generate cfg_schema.h',
                        default=YANG_MODELS_DIR)
    parser.add_argument('-o',
                        dest='output',
                        metavar='output file',
                        type=str,
                        help='output file path for cfg_schema.h',
                        default=DEFAULT_OUTPUT_FILE)

    args = parser.parse_args()
    yang_dir = args.dir
    sy = sonic_yang.SonicYang(yang_dir)
    sy.loadYangModel()
    keys = [k for k, v in sy.confDbYangMap.items() if "container" in v]
    write_cfg_schema(keys, args.output)

if __name__ == "__main__":
    main()
