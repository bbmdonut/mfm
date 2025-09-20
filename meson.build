project(
  'mfm',
  ['c', 'cpp'],
  meson_version: '>= 1.3.0',
  version: '0.1',
  default_options: [
    'buildtype=release',
    'debug=true',
    'c_std=c99',
    'cpp_std=c++11',
    'warning_level=3',
  ],
)

add_project_arguments(['-D_XOPEN_SOURCE=600', '-D_FILE_OFFSET_BITS'], language: 'c')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: true)
rt_dep = cc.find_library('rt', required: true)
thread_dep = dependency('threads')

libprussdrv_proj = subproject('am335_pru')
libprussdrv_dep = libprussdrv_proj.get_variable('libprussdrv_dep')

# libiberty doesn't have a pkg-config file so we can't descover it like some other libs
liberty_dep = cc.find_library('iberty', required: false)
foreach liberty : ['liberty', 'bfd']
  if not liberty_dep.found()
    liberty_dep = cc.find_library(liberty, required: false)
  endif
endforeach

base_dependencies = [thread_dep, m_dep, liberty_dep, rt_dep]

full_dependencies = base_dependencies
if liberty_dep.found()
  full_dependencies += liberty_dep
  liberty_include = include_directories('/usr/include/libiberty')
endif

subdir('mfm')

util_exe = executable(
  'mfm_util',
  mfm_utils_src,
  dependencies: base_dependencies,
  include_directories: [mfm_include, liberty_include],
  install: true,
)

ext2emu_exe = executable(
  'ext2emu',
  mfm_utils_src,
  dependencies: base_dependencies,
  include_directories: [mfm_include, liberty_include],
  install: true,
)

mfm_write_exe = executable(
  'mfm_write',
  mfm_sources_3,
  dependencies : [ base_dependencies, libprussdrv_dep ],
  include_directories: mfm_include,
  install : true,
)

find_crc_info_exe = executable(
  'find_crc_info',
  find_crc_info_src,
  dependencies: full_dependencies,
  include_directories: [mfm_include, liberty_include],
  install: true,
)

#subdir('emu')
#emu = executable(
#  'mfm_emu',
#  mfm_emu_sources + emu_sources,
#  dependencies : [ base_dependencies, libprussdrv_dep ],
#  include_directories: [emu_include, mfm_include],
#  install: true,
#)
