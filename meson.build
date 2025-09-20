project(
  'mfm',
  ['c', 'cpp'],
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['buildtype=release', 'debug=true', 'c_std=c99', 'cpp_std=c++11', 'warning_level=3'],
)

add_project_arguments([
  '-D_XOPEN_SOURCE=600',
  '-D_FILE_OFFSET_BITS'
], language: 'c')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)
rt_dep = cc.find_library('rt', required: false)
liberty_dep = cc.find_library('libiberty', required: true)
thread_dep = dependency('threads')
 
add_project_arguments('-I/usr/include/libiberty', language: 'c')

dependencies = [
  thread_dep,
  m_dep,
  liberty_dep,
  rt_dep
]

subdir('mfm')

util_exe = executable(
  'mfm_util',
  mfm_utils_src,
  dependencies : dependencies,
  include_directories: mfm_include,
  install : false,
)

ext2emu_exe = executable(
  'ext2emu',
  mfm_utils_src,
  dependencies : dependencies,
  include_directories: mfm_include,
  install : false,
)

# fails because we don't have prussdrive.h
# mfm_write_exe = executable(
#   'mfm_write',
#   mfm_sources_3,
#   dependencies : dependencies,
#   include_directories: mfm_include,
#   install : false,
# )

find_crc_info_exe = executable(
  'find_crc_info',
  find_crc_info_src,
  dependencies : dependencies,
  include_directories: mfm_include,
  install : false,
)

