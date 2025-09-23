# Unit tests using Unity framework
unity_dep = dependency('unity')

# Unit tests for core modules (with manual main functions for now)
unit_tests = [
  'crc_ecc',
  'msg',
]

foreach test_name : unit_tests
  # Build test executable with manual main
  test_exe = executable(
    'test_' + test_name,
    ['unit/test_' + test_name + '.c', '../mfm/' + test_name + '.c'],
    include_directories: mfm_include,
    dependencies: [base_dependencies, unity_dep],
    build_by_default: false
  )

  test(test_name + ' unit tests', test_exe)
endforeach

# Simple parse_cmdline test (has complex dependencies)
parse_cmdline_test = executable(
  'test_parse_cmdline',
  'unit/test_parse_cmdline.c',
  include_directories: mfm_include,
  dependencies: [base_dependencies, unity_dep],
  build_by_default: false
)
test('parse_cmdline unit tests', parse_cmdline_test)

# Integration tests - test that executables can be run
# Note: These test the --help flag which should work even without hardware

#test('mfm_util help', util_exe, args: ['--help'], should_fail: false)
#test('ext2emu help', ext2emu_exe, args: ['--help'], should_fail: false)
#test('find_crc_info help', find_crc_info_exe, args: ['--help'], should_fail: false)
#test('mfm_write help', mfm_write_exe, args: ['--help'], should_fail: false)

## Test built executables exist and are runnable
#test('util_exe exists', util_exe, args: ['--version'], should_fail: true)  # Most utils exit 1 on --version
#test('ext2emu exists', ext2emu_exe, args: ['--version'], should_fail: true)
