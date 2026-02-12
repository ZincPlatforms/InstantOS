# Kernel source files
# Find all source files using run_command
c_files_output = run_command('find', '.', '-name', '*.c', '-type', 'f', check: true).stdout().strip()
cpp_files_output = run_command('find', '.', '-name', '*.cpp', '-type', 'f', check: true).stdout().strip()
cpp_modules_output = run_command('find', '.', '-name', '*.cppm', '-type', 'f', check: true).stdout().strip()
asm_files_output = run_command('find', '.', '-name', '*.asm', '-type', 'f', check: true).stdout().strip()
uacpi_files_output = run_command('find', '../outside/uacpi/source', '-name', '*.c', '-type', 'f', check: true).stdout().strip()

# Split and filter empty strings
c_sources_list = []
foreach f : c_files_output.split('\n')
  if f != ''
    c_sources_list += [f]
  endif
endforeach

cpp_sources_list = []
foreach f : cpp_files_output.split('\n')
  if f != ''
    cpp_sources_list += [f]
  endif
endforeach

foreach f : cpp_modules_output.split('\n')
  if f != ''
    cpp_sources_list += [f]
  endif
endforeach


asm_sources_list = []
foreach f : asm_files_output.split('\n')
  if f != ''
    asm_sources_list += [f]
  endif
endforeach

uacpi_sources_list = []
foreach f : uacpi_files_output.split('\n')
  if f != ''
    uacpi_sources_list += [f]
  endif
endforeach

# Combine C sources
all_c_sources = c_sources_list + uacpi_sources_list

# Compile C sources
c_objects = []
c_idx = 0
foreach src : all_c_sources
  obj_name = 'c_@0@.o'.format(c_idx)
  c_objects += custom_target(obj_name,
    input: files(src),
    output: obj_name,
    command: [
      'x86_64-elf-gcc',
      common_flags,
      arch_flags,
      '-std=c11',
      '-I' + meson.current_source_dir(),
      '-I' + meson.project_source_root() / 'outside/limine',
      '-I' + meson.project_source_root() / 'outside/uacpi/include',
      '-I' + meson.project_source_root() / 'outside/freestnd-c-hdrs/x86_64/include',
      '-I' + meson.project_source_root() / 'src/cstd/include',
      '-isystem', meson.project_source_root() / 'src/cxxstd/include',
      '-c', '@INPUT@',
      '-o', '@OUTPUT@',
    ],
  )
  c_idx += 1
endforeach

# Compile C++ sources
cpp_objects = []
cpp_idx = 0
foreach src : cpp_sources_list
  obj_name = 'cpp_@0@.o'.format(cpp_idx)
  cpp_objects += custom_target(obj_name,
    input: files(src),
    output: obj_name,
    command: [
      'x86_64-elf-g++',
      common_flags,
      arch_flags,
      cpp_flags,
      '-std=c++23',
      '-I' + meson.current_source_dir(),
      '-I' + meson.project_source_root() / 'outside/limine',
      '-I' + meson.project_source_root() / 'outside/uacpi/include',
      '-I' + meson.project_source_root() / 'outside/freestnd-c-hdrs/x86_64/include',
      '-I' + meson.project_source_root() / 'freestanding/cxx/include',
      '-I' + meson.project_source_root() / 'src/cstd/include',
      '-isystem', meson.project_source_root() / 'src/cxxstd/include',
      '-c', '@INPUT@',
      '-o', '@OUTPUT@',
    ],
  )
  cpp_idx += 1
endforeach

# Compile assembly sources
asm_objects = []
asm_idx = 0
foreach src : asm_sources_list
  obj_name = 'asm_@0@.o'.format(asm_idx)
  asm_objects += custom_target(obj_name,
    input: files(src),
    output: obj_name,
    command: [
      nasm,
      '-felf64',
      '-g',
      '-F', 'dwarf',
      '-Wall',
      '@INPUT@',
      '-o', '@OUTPUT@',
    ],
  )
  asm_idx += 1
endforeach

kernel_objects = c_objects + cpp_objects + asm_objects
