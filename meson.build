project('instant', ['c', 'cpp'],
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'cpp_std=c++23',
    'warning_level=3',
    'werror=false',
    'buildtype=debug',
  ]
)

nasm = find_program('nasm')
xorriso = find_program('xorriso', required: false)
objcopy = find_program('objcopy')

build_dir = meson.current_build_dir()
iso_root = build_dir / 'iso_root'

common_flags = [
  '-nostdinc',
  '-ffreestanding',
  '-fno-stack-protector',
  '-fno-stack-check',
  '-fno-lto',
  '-fno-PIC',
  '-ffunction-sections',
  '-fdata-sections',
]

arch_flags = [
  '-m64',
  '-march=x86-64',
  '-mabi=sysv',
  '-mno-80387',
  '-mno-mmx',
  '-mno-sse',
  '-mno-sse2',
  '-mno-red-zone',
  '-mcmodel=kernel',
]

cpp_flags = [
  '-fno-rtti',
  '-fno-exceptions',
]

inc_dirs = include_directories(
  'src',
  'outside/limine',
  'outside/uacpi/include',
  'freestanding/c/include',
  'src/cstd/include',
)

inc_dirs_system = include_directories(
  'src/cxxstd/include',
  is_system: true,
)

inc_dirs_freestanding_cxx = include_directories(
  'freestanding/cxx/include',
)

subdir('ilibcxx')

subdir('tools')
subdir('userland')
subdir('src')

kernel = custom_target('instant',
  input: kernel_objects,
  output: 'instant',
  command: [
    'ld',
    '-m', 'elf_x86_64',
    '-nostdlib',
    '-static',
    '-z', 'max-page-size=0x1000',
    '--gc-sections',
    '-T', meson.current_source_dir() / 'linker.ld',
    '-o', '@OUTPUT@',
    '@INPUT@',
  ],
  build_by_default: true,
  install: false,
)

make_prog = find_program('make')
limine_binary = custom_target('limine',
  output: 'limine',
  command: [
    'sh', '-c',
    make_prog.full_path() + ' -C ' + meson.current_source_dir() / 'outside/limine' + ' limine && cp ' + meson.current_source_dir() / 'outside/limine/limine' + ' @OUTPUT@'
  ],
  build_by_default: true,
  install: false,
  console: true,
)

if xorriso.found()
  iso = custom_target('iso',
    input: [kernel, initrd, limine_binary],
    output: 'instant.iso',
    command: [
      find_program('scripts/make_iso.sh'),
      '@OUTPUT@',
      '@INPUT0@',
      '@INPUT1@',
      meson.current_source_dir(),
      iso_root,
      '@INPUT2@',
    ],
    depends: [limine_binary],
    build_by_default: true,
    install: false,
    console: true,
  )

  qemu = find_program('qemu-system-x86_64', required: false)
  if qemu.found()
    run_qemu = custom_target('run',
      input: [iso],
      output: 'qemu_run.log',
      command: [
        qemu,
        '-cdrom', '@INPUT@',
        '-m', '256M',
        '-enable-kvm',
        '-cpu', 'host',
        '-serial', 'stdio',
      ],
      build_by_default: false,
      console: true,
    )

    run_qemu_debug = custom_target('debug',
      input: [iso],
      output: 'qemu_debug.log',
      command: [
        qemu,
        '-cdrom', '@INPUT@',
        '-m', '256M',
        '-enable-kvm',
        '-serial', 'stdio',
        '-no-reboot',
        '-no-shutdown',
        '-s', '-S'
      ],
      build_by_default: false,
      console: true,
    )

    message('QEMU targets available:')
    message('  meson compile -C build run    - Run InstantOS in QEMU')
    message('  meson compile -C build debug  - Run InstantOS in QEMU with GDB server')
  else
    message('QEMU not found - run targets disabled')
  endif

  alias_target('everything', [kernel, initrd, limine_binary, iso])
else
  alias_target('everything', [kernel, initrd, limine_binary])

  limine_clean = custom_target('clean-limine',
    output: 'limine_clean.log',
    command: [
      make_prog,
      '-C', meson.current_source_dir() / 'outside/limine',
      'clean'
    ],
    build_by_default: false,
    console: true,
  )
endif
