#this comment is just for highlight
import os
cc='g++'
cc_flags=['-g','-O2','-Wall', '-std=gnu++98']
libs = ['m', 'pthread']
lib_path = []
link_flags = []

include_path = [
  'include',
  'FastCGI',
  'thrift',
  'thrift/concurrency',
  'thrift/processor',
  'thrift/protocol',
  'thrift/server',
  'thrift/transport',
]

source_path = [
  'src',
  'thrift',
  'FastCGI',
  'thrift/concurrency',
  'thrift/processor',
  'thrift/protocol',
  'thrift/server',
  'thrift/transport',
]

def get_all_source_files():
  objs = []
  for path in source_path:
    for file in os.listdir(path):
      if (file[-3:] != ".cc" and file[-4:] != ".cpp") or file[-7:] == "test.cc":
        continue
      objs.append(path + "/" + file)
  return objs

all_source_files = get_all_source_files()

env = Environment()
env.Append(CC = cc)
env.Append(CCFLAGS = cc_flags)
env.Append(CPPPATH = include_path)
env.Append(LIBS = libs)
env.Append(LIBPATH = lib_path)
env.Append(LINKFLAGS = link_flags)
env.Program('GI', all_source_files)

