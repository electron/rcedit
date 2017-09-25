{
  'targets': [
    {
      'target_name': 'rcedit',
      'type': 'executable',
      'sources': [
        'src/main.cc',
        'src/rescle.cc',
        'src/rescle.h',
        'src/rcedit.rc',
        'src/version.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # console executable
        },
        'VCCLCompilerTool': {
          'Optimization': 3, # /Ox, full optimization
          'FavorSizeOrSpeed': 1, # /Os, favour small code
          'RuntimeLibrary': 0, # /MT, for statically linked runtime
        },
      },
      'conditions': [
        ['MSVS_VERSION=="2015"', {
          'msbuild_toolset': 'v140_xp',
        }],
        ['MSVS_VERSION=="2017"', {
          'msbuild_toolset': 'v141_xp',
        }],
      ],
      'configurations': {
        'Default': {
        },
        'Default_x64': {
          'msvs_configuration_platform': 'x64',
        },
      }
    },
  ],
}
