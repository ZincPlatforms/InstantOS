userland_programs = []

files_result = run_command('find', '.', '-maxdepth', '1', '-name', '*.cpp', '-type', 'f', check: false)
if files_result.returncode() == 0 and files_result.stdout().strip() != ''
  files = files_result.stdout().strip().split('\n')

  foreach file : files
    if file != '' and file.endswith('.cpp')
      clean_file = file
      if clean_file.startswith('./')
        clean_file = clean_file.substring(2)
      endif
      prog_name = clean_file.split('.')[0]

      obj_target = custom_target(prog_name + '_obj',
        input: files(clean_file),
        output: prog_name + '.o',
        command: [
          'x86_64-elf-g++',
          '-nostdlib',
          '-nostdinc',
          '-ffreestanding',
          '-fno-stack-protector',
          '-fno-builtin',
          '-fno-stack-check',
          '-fno-lto',
          '-fno-PIC',
          '-fno-rtti',
          '-fno-exceptions',
          '-m64',
          '-march=x86-64',
          '-mabi=sysv',
          '-mno-red-zone',
          '-mcmodel=small',
          '-std=c++17',
          '-I@0@'.format(meson.project_source_root() / 'ilibcxx/include'),
          '-I@0@'.format(meson.project_source_root() / 'outside/freestnd-c-hdrs/x86_64/include'),
          '-I@0@'.format(meson.project_source_root() / 'freestanding/cxx/include'),
          '-c',
          '@INPUT@',
          '-o', '@OUTPUT@'
        ],
        build_by_default: true,
      )

      prog = custom_target(prog_name,
        input: [obj_target, crt0_o, libilibcxx],
        output: prog_name,
        command: [
          'x86_64-elf-ld',
          '-static',
          '-nostdlib',
          '--entry=_start',
          '@INPUT0@',
          '@INPUT1@',
          '--start-group',
          '@INPUT2@',
          '--end-group',
          '-o', '@OUTPUT@'
        ],
        build_by_default: true,
        install: false,
      )

      userland_programs += prog
    endif
  endforeach
endif

if userland_programs.length() > 0
  initrd = custom_target('initrd.img',
    input: userland_programs,
    output: 'initrd.img',
    command: [
      mkinitrd,
      '@OUTPUT@',
      '@INPUT@'
    ],
    build_by_default: true,
    install: false,
  )
else

  initrd = custom_target('initrd.img',
    output: 'initrd.img',
    command: ['touch', '@OUTPUT@'],
    build_by_default: true,
    install: false,
  )
endif
