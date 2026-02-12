ilibcxx_sources = files(
  'src/cxx/cxxabi.cpp',
  'src/cxx/new.cpp',
  'src/cxx/iostream.cpp',
  'src/cxx/exception.cpp',
  'src/cxx/string.cpp',
  'src/cxx/cstdio.cpp',
  'src/cxx/cstdlib.cpp',
  'src/cxx/unistd.cpp',
  'src/cxx/exit.cpp',
)

nasm = find_program('nasm')

crt0_o = custom_target('crt0.o',
  input: 'src/asm/crt0.asm',
  output: 'crt0.o',
  command: [nasm, '-felf64', '@INPUT@', '-o', '@OUTPUT@'],
)

syscall_o = custom_target('syscall.o',
  input: 'src/asm/syscall.asm',
  output: 'syscall.o',
  command: [nasm, '-felf64', '@INPUT@', '-o', '@OUTPUT@'],
)

libilibcxx = static_library('ilibcxx',
  sources: ilibcxx_sources + [syscall_o],
  cpp_args: ['-fno-exceptions', '-fno-rtti', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables'],
  include_directories: include_directories('include'),
  pic: false,
)

crt0 = declare_dependency(
  sources: [crt0_o],
  include_directories: include_directories('include'),
)
