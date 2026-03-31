project('rgt-race-postprocessor', 'cpp', default_options: ['cpp_std=c++20'])

cpp = meson.get_compiler('cpp')

devkit_dep = dependency('rgt-devkit',
  fallback: ['rgt-devkit', 'devkit_dep'],
  required: true
)

poco_foundation = cpp.find_library('PocoFoundation', required: true)
poco_util = cpp.find_library('PocoUtil', required: true)
poco_net = cpp.find_library('PocoNet', required: true)
poco_json = cpp.find_library('PocoJSON', required: true)
poco_data = cpp.find_library('PocoData', required: false) 
poco_data_postgresql = cpp.find_library('PocoDataPostgreSQL', required: false)
poco_jwt = cpp.find_library('PocoJWT', required: true)
poco_redis = cpp.find_library('PocoRedis', required: true)
poco_xml = cpp.find_library('PocoXML', required: true)

pq = cpp.find_library('pq', required: true)
sodium = cpp.find_library('sodium', required: true)
ssl = cpp.find_library('ssl', required: true)
crypto = cpp.find_library('crypto', required: true)

aws_core = cpp.find_library('aws-cpp-sdk-core', required: true)
aws_s3 = cpp.find_library('aws-cpp-sdk-s3', required: true)

all_deps = [
  poco_foundation,
  poco_util,
  poco_net,
  poco_json,
  poco_jwt,
  poco_redis,
  poco_xml,
  pq,
  sodium,
  ssl,
  crypto,
  devkit_dep,
  aws_core,
  aws_s3,
]

if poco_data.found()
  all_deps += poco_data
endif

if poco_data_postgresql.found()
  all_deps += poco_data_postgresql
endif

src_files = [
  'src/app/main.cpp',
  'src/platform/Utils.cpp',
  'src/platform/RacePostprocessorServer.cpp',
]

include_dirs = [
  'src/platform'
]

executable('rgt-race-postprocessor',
  src_files,
  include_directories: include_dirs,
  dependencies: all_deps,
    link_args: [
    '-Wl,-rpath,@executable_path/../lib',      # ищет рядом с бинарником
    '-Wl,-rpath,/opt/homebrew/lib',            # Homebrew на Apple Silicon
    '-Wl,-rpath,/usr/local/lib',               # Homebrew на Intel Mac
  ]
)