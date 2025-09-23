# EMU sources
emu_sources = files(
  'mfm_emu.c',
  'parse_cmdline.c'
)

# MFM sources referenced by the emu
mfm_emu_sources = files(
  '../mfm/pru_setup.c',
  '../mfm/msg.c',
  '../mfm/emu_tran_file.c',
  '../mfm/crc_ecc.c',
  '../mfm/board.c'
)

emu_include = include_directories('inc')

# Dependencies for the emulator
emu_deps = [thread_dep, libprussdrv_dep, m_dep, rt_dep]

# Build the mfm_emu executable
mfm_emu_exe = executable(
  'mfm_emu',
  emu_sources + mfm_emu_sources,
  dependencies: emu_deps,
  include_directories: [emu_include, mfm_include],
  c_args: ['-O3', '-g', '-Wall', '-D_FILE_OFFSET_BITS=64'],
  install: true,
)

# PRU assembly targets - get pasm from subproject
am335_pru_proj = subproject('am335_pru')
pasm_exe = am335_pru_proj.get_variable('pasm_exe')

pasm_args = ['-V2', '-b', '-L']
pru_deps = files('prucode.hp', 'inc/cmd.h')

# Build PRU code variants
pru_variants = ['reva', 'revb', 'revc']
pru_cores = ['prucode0', 'prucode1']

foreach core : pru_cores
  foreach variant : pru_variants
    pru_define = []
    if variant == 'revb'
      pru_define = ['-DREVB']
    elif variant == 'revc'
      pru_define = ['-DREVC']
    endif

    pru_target = core + '_' + variant + '.bin'

    custom_target(
      pru_target,
      input: [core + '.p'] + pru_deps,
      output: pru_target,
      command: [pasm_exe] + pasm_args + pru_define + ['@INPUT0@', core + '_' + variant],
      build_by_default: true,
    )
  endforeach
endforeach
