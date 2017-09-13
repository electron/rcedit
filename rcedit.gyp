{
  'variables': {
    'conditions': [
      ['OS=="win" and (MSVS_VERSION=="2015")', {
        'msbuild_toolset': 'v140_xp',
      }],
      ['OS=="win" and (MSVS_VERSION=="2017")', {
        'msbuild_toolset': 'v141_xp',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'rcedit',
      'type': 'executable',
      'sources': [
        'src/main.cc',
        'src/rescle.cc',
        'src/rescle.h',
        'src/rcedit.rc',
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
        ['OS=="win" and "<(msbuild_toolset)"!=""', {
          'msbuild_toolset': '<(msbuild_toolset)',
        }],
      ],
    },
  ],
}
