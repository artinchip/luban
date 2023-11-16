import os
import sys
import awtk_locator as locator
from SCons import Script

def Helper(ARGUMENTS):
    locator.init(ARGUMENTS)

    from app_helper_base import AppHelperBase
    return AppHelperBase(ARGUMENTS)

def prepare_depends_libs(ARGUMENTS, helper, libs):
    if ARGUMENTS.get('PREPARE_DEPENDS', '').lower().startswith('f'):
        return

    args = ' AWTK_ROOT=' + helper.AWTK_ROOT
    if helper.MVVM_ROOT:
        args += ' MVVM_ROOT=' + helper.MVVM_ROOT

    for key in ARGUMENTS:
        if not key == 'AWTK_ROOT' and not key == 'MVVM_ROOT':
            args += ' ' + key + '=' + ARGUMENTS[key]

    num_jobs_str = ''
    num_jobs = Script.GetOption('num_jobs')
    if num_jobs > 1:
        num_jobs_str = ' -j' + str(num_jobs)

    clean_str = ''
    if Script.GetOption('clean'):
        clean_str = ' -c '

    for lib in libs:
        if 'root' in lib and os.path.exists(lib['root'] + '/SConstruct'):
            cmd = 'cd ' + lib['root'] + ' && scons' + clean_str + num_jobs_str + args
            print(cmd)
            result = os.system(cmd)
            if not result == 0:
                sys.exit(result)
