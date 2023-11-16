﻿import os
import sys
import stat
import re
import copy
import glob
import shutil
import platform
import subprocess

###########################
DPI = 'x1'
ACTION = 'all'
ASSET_C = ''
BIN_DIR = ''
THEMES = ['default']
ASSETS_ROOT = ''
AWTK_ROOT = ''
OUTPUT_ROOT = None
INPUT_DIR = ''
OUTPUT_DIR = ''
APP_THEME = 'default'
THEME = 'default'
THEME_PACKAGED = True
IMAGEGEN_OPTIONS = 'bgra+bgr565'
LCD_ORIENTATION = '0'
LCD_FAST_ROTATION_MODE = False
ON_GENERATE_RES_BEFORE = None
ON_GENERATE_RES_AFTER = None
EXEC_CMD_HANDLER = None
IS_EXCLUDED_FILE_HANDLER = None
IS_GENERATE_RAW = True
IS_GENERATE_INC_RES = True
IS_GENERATE_INC_BITMAP = True
ASSETS_SUBNAME = '__assets_'
###########################


OS_NAME = platform.system()


def get_action():
    return ACTION

def set_action(action):
    global ACTION
    ACTION = action

def get_assets_subname():
    return ASSETS_SUBNAME

def set_assets_subname(subname):
    global ASSETS_SUBNAME
    ASSETS_SUBNAME = subname

def get_asset_c():
    return ASSET_C

def set_asset_c(asset_c):
    global ASSET_C
    ASSET_C = asset_c

def get_output_root():
    return OUTPUT_ROOT

def get_assets_root():
    return ASSETS_ROOT

def on_generate_res_before(handler):
    global ON_GENERATE_RES_BEFORE
    ON_GENERATE_RES_BEFORE = handler


def on_generate_res_after(handler):
    global ON_GENERATE_RES_AFTER
    ON_GENERATE_RES_AFTER = handler


def emit_generate_res_before(type):
    if ON_GENERATE_RES_BEFORE != None:
        ctx = {
            'type': type,
            'theme': THEME,
            'imagegen_options': IMAGEGEN_OPTIONS,
            'lcd_orientation' : LCD_ORIENTATION,
            'lcd_fast_rotation_mode' : LCD_FAST_ROTATION_MODE,
            'input': INPUT_DIR,
            'output': OUTPUT_DIR,
            'awtk_root': AWTK_ROOT
        }
        ON_GENERATE_RES_BEFORE(ctx)


def emit_generate_res_after(type):
    if ON_GENERATE_RES_AFTER != None:
        ctx = {
            'type': type,
            'theme': THEME,
            'imagegen_options': IMAGEGEN_OPTIONS,
            'lcd_orientation' : LCD_ORIENTATION,
            'lcd_fast_rotation_mode' : LCD_FAST_ROTATION_MODE,
            'input': INPUT_DIR,
            'output': OUTPUT_DIR,
            'awtk_root': AWTK_ROOT
        }
        ON_GENERATE_RES_AFTER(ctx)


def set_is_excluded_file_handler(handler):
    global IS_EXCLUDED_FILE_HANDLER
    IS_EXCLUDED_FILE_HANDLER = handler


def set_exec_command_handler(handler):
    global EXEC_CMD_HANDLER
    EXEC_CMD_HANDLER = handler


def set_tools_dir(dir):
    global BIN_DIR
    BIN_DIR = dir

def set_lcd_rortrail_info(lcd_orientation, lcd_fast_rotation_mode):
    global LCD_ORIENTATION
    global LCD_FAST_ROTATION_MODE
    LCD_ORIENTATION = lcd_orientation
    LCD_FAST_ROTATION_MODE = lcd_fast_rotation_mode

def set_dpi(dpi):
    global DPI
    DPI = dpi


def set_enable_generate_raw(enable):
    global IS_GENERATE_RAW
    IS_GENERATE_RAW = enable


def set_enable_generate_inc_res(enable):
    global IS_GENERATE_INC_RES
    IS_GENERATE_INC_RES = enable


def set_enable_generate_inc_bitmap(enable):
    global IS_GENERATE_INC_BITMAP
    IS_GENERATE_INC_BITMAP = enable


def set_app_theme(theme):
    global APP_THEME
    APP_THEME = theme


def set_current_theme(index):
    global THEME
    global THEME_PACKAGED
    global IMAGEGEN_OPTIONS
    global INPUT_DIR
    global OUTPUT_DIR

    if index >= len(THEMES):
        return

    theme = THEMES[index]

    if isinstance(theme, str):
        THEME = theme
    elif isinstance(theme, dict):
        THEME = theme['name']
        THEME_PACKAGED = True
        IMAGEGEN_OPTIONS = 'bgra+bgr565'
        if 'imagegen_options' in theme:
            IMAGEGEN_OPTIONS = theme['imagegen_options']
        if 'packaged' in theme:
            THEME_PACKAGED = theme['packaged']

    INPUT_DIR = join_path(ASSETS_ROOT, THEME+'/raw')
    if not os.path.exists(INPUT_DIR):
        INPUT_DIR = join_path(ASSETS_ROOT, THEME)

    OUTPUT_DIR = join_path(OUTPUT_ROOT, THEME)


def theme_foreach(visit, ctx = None):
    for i in range(len(THEMES)):
        set_current_theme(i)

        if ctx != None:
            visit(ctx)
        else:
            visit()


def get_imagegen_options_of_default_theme():
    opt = 'bgra+bgr565'

    if len(THEMES) > 0:
        if isinstance(THEMES[0], str):
            opt = IMAGEGEN_OPTIONS
        elif isinstance(THEMES[0], dict):
            for iter in THEMES:
                if iter['name'] == 'default':
                    if 'imagegen_options' in iter:
                        opt = iter['imagegen_options']
                    break
    return opt


def is_packaged_of_default_theme():
    packaged = True

    if len(THEMES) > 0:
        if isinstance(THEMES[0], str):
            packaged = THEME_PACKAGED
        elif isinstance(THEMES[0], dict):
            for iter in THEMES:
                if iter['name'] == 'default':
                    if 'packaged' in iter:
                        packaged = iter['packaged']
                    break
    return packaged


def getcwd():
    if sys.version_info >= (3, 0):
        return os.getcwd()
    else:
        return os.getcwdu()


def to_file_system_coding(s):
    if sys.version_info >= (3, 0): return s
    coding = sys.getfilesystemencoding()
    return s.encode(coding)


def join_path(root, subdir):
    return os.path.normpath(os.path.join(root, subdir))


def make_dirs(path):
    if not os.path.exists(path):
        os.makedirs(path)


def copy_dir(src, dst):
    if not os.path.exists(src):
        print('copy directory: ' + src + ' is not exists.')
    else:
        print('copy directory: ' + src + ' ==> ' + dst)
        shutil.copytree(src, dst)


def copy_file(src, dst):
    if not os.path.exists(src):
        print('copy file: ' + src + ' is not exists.')
    else:
        print('copy file: ' + src + ' ==> ' + dst)
        shutil.copy(src, dst)


def remove_dir(dir):
    if os.path.isdir(dir):
        for root, dirs, files in os.walk(dir):
            for f in files:
                real_path = join_path(root, f)
                os.chmod(real_path, stat.S_IRWXU)

        shutil.rmtree(dir)
    elif os.path.isfile(dir):
        os.chmod(dir, stat.S_IRWXU)
        os.remove(dir)
    else:
        print('dir ' + dir + ' not exist')


def get_appint_folder_ex(path, regex = '/', folder_list = [], parent = ''):
    if not os.path.exists(path) or not os.path.isdir(path):
        return folder_list

    for f in os.listdir(path):
        src = join_path(path, f)
        if os.path.isdir(src) and f != '.' and f != '..':
            file_path = os.path.join(parent, f)
            if regex == '/':
                folder_list.append(tuple((src, file_path)))
            folder_list = get_appint_folder_ex(src, regex, folder_list, file_path)
        elif (regex == os.path.splitext(f)[1]):
            file_path = os.path.join(parent, f)
            folder_list.append(tuple((src, file_path)))
    return folder_list


def get_appint_folder(path, regex = '/', include_self = True):
    folder_list = []
    regex_list = regex.split("|")

    if not os.path.exists(path) or not os.path.isdir(path):
        return folder_list

    for reg in regex_list:
        folder_list += get_appint_folder_ex(path, reg, [], '')
    
    if include_self == True:
        folder_list.append(tuple((path, '')))
    return folder_list


def to_exe(name):
    if OS_NAME == 'Windows':
        return join_path(BIN_DIR, name+'.exe')
    else:
        return join_path(BIN_DIR, name)


def glob_asset_files(path):
    if not path.endswith(os.sep + 'ui' + os.sep + '*.data'):
        result = glob.glob(path)
    else:
        result = []
        dir = path[0 : len(path) - 7]
        if os.path.isdir(dir):
            for root, dirs, files in os.walk(dir):
                for f in files:
                    filename, extname = os.path.splitext(f)
                    if extname == '.data':
                        result.append(join_path(root, f))

    return result


def write_file(filename, s):
    if sys.version_info >= (3, 0):
        with open(filename, 'w', encoding='utf8') as f:
            f.write(s)
    else:
        with open(filename, 'w') as f:
            s = s.encode('utf8')
            f.write(s)


def is_excluded_file(filename):
    return (IS_EXCLUDED_FILE_HANDLER and IS_EXCLUDED_FILE_HANDLER(filename))


def to_asset_const_name(filename, root):
    basename = os.path.splitext(filename)[0]
    basename = basename.replace(root, '.')
    basename = basename.replace('\\', '/')
    basename = basename.replace('/fonts/', '/font/')
    basename = basename.replace('/images/', '/image/')
    basename = basename.replace('/styles/', '/style/')
    basename = basename.replace('/scripts/', '/script/')
    basename = basename.replace('/flows/', '/flow/')
    basename = basename.replace('./', '')
    basename = re.sub(r'[^a-zA-Z0-9]', '_', basename)
    return basename


def build_all():
    os.system('scons')


def exec_cmd(cmd):
    if EXEC_CMD_HANDLER:
        EXEC_CMD_HANDLER(cmd)
    else:
        print(cmd)
        # 转为文件系统的编码格式，避免中文乱码
        cmd = to_file_system_coding(cmd)
        # 启动子进程
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
        exe_name = cmd.split(' ', 1)[0]
        out_str = p.communicate()[0]
        if out_str != None:
            out_str = out_str.decode('UTF-8')
        # 将输出信息重新打印到控制台
        print(out_str)
        sys.stdout.flush()
        # 如果程序出现异常或某个资源打包失败则直接退出
        if p.returncode != 0:
            sys.exit(exe_name + ' exit code:' + str(p.returncode))
        if 'gen fail' in out_str:
            sys.exit(1)


def themegen(raw, inc, theme):
    exec_cmd('\"' + to_exe('themegen') + '\" \"' + raw + '\" \"' + inc + '\" data ' + theme)


def themegen_bin(raw, bin):
    exec_cmd('\"' + to_exe('themegen') + '\" \"' + raw + '\" \"' + bin + '\" bin')


def strgen(raw, inc, theme):
    if(os.path.isfile(raw)):
        exec_cmd('\"' + to_exe('strgen') + '\" \"' + raw + '\" \"' + inc + '\" data ' + theme)


def strgen_bin(raw, bin):
    if(os.path.isfile(raw)):
        exec_cmd('\"' + to_exe('strgen') + '\" \"' + raw + '\" \"' + bin + '\" bin')

def resgen(raw, inc, theme, outExtname):
    exec_cmd('\"' + to_exe('resgen') + '\" \"' + raw + '\" \"' + inc + '\" ' + theme + ' ' + outExtname)


def fontgen(raw, text, inc, size, options, theme):
    fontgenName = 'fontgen'
    if options == 'mono' and os.path.exists(to_exe('fontgen_ft')):
        fontgenName = 'fontgen_ft'

    exec_cmd('\"' + to_exe(fontgenName) + '\" \"' + raw + '\" \"' + text + '\" \"' + inc + '\" ' +
        str(size) + ' ' + options + ' ' + theme)


def imagegen(raw, inc, options, theme, lcd_orientation, lcd_fast_rotation_mode):
    if not lcd_fast_rotation_mode :
        lcd_orientation = '0'
    exec_cmd('\"' + to_exe('imagegen') + '\" \"' + raw + '\" \"' + inc + '\" ' + options + ' ' + theme + ' ' + lcd_orientation)


def svggen(raw, inc, theme):
    exec_cmd('\"' + to_exe('bsvggen') + '\" \"' + raw + '\" \"' + inc + '\" data ' + theme)


def svggen_bin(raw, bin):
    exec_cmd('\"' + to_exe('bsvggen') + '\" \"' + raw + '\" \"' + bin + '\" bin')


def xml_to_ui(raw, inc, theme):
    exec_cmd('\"' + to_exe('xml_to_ui') + '\" \"' + raw + '\" \"' + inc + '\" data \"\" ' + theme)


def xml_to_ui_bin(raw, bin):
    exec_cmd('\"' + to_exe('xml_to_ui') + '\" \"' + raw + '\" \"' + bin + '\" bin')


def gen_res_all_style():
    if not THEME_PACKAGED and THEME != 'default':
        return

    raw = join_path(INPUT_DIR, 'styles')
    if not os.path.exists(raw):
        return

    emit_generate_res_before('style')

    if IS_GENERATE_RAW:
        bin = join_path(OUTPUT_DIR, 'raw/styles')
        make_dirs(bin)
        themegen_bin(raw, bin)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/styles')
        make_dirs(inc)
        themegen(raw, inc, THEME)

    emit_generate_res_after('style')


def gen_res_svg():
    raw = join_path(INPUT_DIR, 'images/svg')
    if not os.path.exists(raw):
        return

    if IS_GENERATE_RAW:
        bin = join_path(OUTPUT_DIR, 'raw/images/svg')
        make_dirs(bin)
        svggen_bin(raw, bin)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/images')
        make_dirs(inc)
        svggen(raw, inc, THEME)


def gen_res_png_jpg():
    if IS_GENERATE_RAW:
        if INPUT_DIR != join_path(OUTPUT_DIR, 'raw'):
            dirs = ['xx', 'x1', 'x2', 'x3']
            for d in dirs:
                raw = join_path(INPUT_DIR, 'images/'+d)
                if os.path.exists(raw):
                    dst = join_path(OUTPUT_DIR, 'raw/images/'+d)
                    if os.path.exists(dst):
                        remove_dir(dst)
                    copy_dir(raw, dst)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/images')
        dirs = [DPI, 'xx']
        for d in dirs:
            raw = join_path(INPUT_DIR, 'images/'+d)
            if os.path.exists(raw):
                make_dirs(inc)
                if IS_GENERATE_INC_RES:
                    resgen(raw, inc, THEME, '.res')
                if IS_GENERATE_INC_BITMAP:
                    imagegen(raw, inc, IMAGEGEN_OPTIONS, THEME, LCD_ORIENTATION, LCD_FAST_ROTATION_MODE)

        # 如果当前主题的gen选项与default主题不一致，则按新的gen选项重新生成图片的位图数据
        if IS_GENERATE_INC_BITMAP and THEME != 'default':
            if get_imagegen_options_of_default_theme() != IMAGEGEN_OPTIONS:
                for d in dirs:
                    raw = join_path(ASSETS_ROOT, 'default/raw')
                    if not os.path.exists(raw):
                        raw = join_path(ASSETS_ROOT, 'default')
                    raw = join_path(raw, 'images/'+d)

                    if os.path.exists(raw):
                        make_dirs(inc)
                        imagegen(raw, inc, IMAGEGEN_OPTIONS, THEME, LCD_ORIENTATION, LCD_FAST_ROTATION_MODE)


def gen_res_all_image():
    if not THEME_PACKAGED and THEME != 'default':
        return

    emit_generate_res_before('image')
    gen_res_png_jpg()
    gen_res_svg()
    emit_generate_res_after('image')


def gen_res_all_ui():
    if not THEME_PACKAGED and THEME != 'default':
        return

    raw = join_path(INPUT_DIR, 'ui')
    if not os.path.exists(raw):
        return

    emit_generate_res_before('ui')

    if IS_GENERATE_RAW:
        bin = join_path(OUTPUT_DIR, 'raw/ui')
        make_dirs(bin)
        xml_to_ui_bin(raw, bin)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/ui')
        make_dirs(inc)
        xml_to_ui(raw, inc, THEME)

    emit_generate_res_after('ui')


def gen_res_all_data():
    if not THEME_PACKAGED and THEME != 'default':
        return

    raw = join_path(INPUT_DIR, 'data')
    if not os.path.exists(raw):
        return

    emit_generate_res_before('data')

    if IS_GENERATE_RAW:
        if INPUT_DIR != join_path(OUTPUT_DIR, 'raw'):
            dst = join_path(OUTPUT_DIR, 'raw/data')
            if os.path.exists(dst):
                remove_dir(dst)
            copy_dir(raw, dst)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/data')
        make_dirs(inc)
        resgen(raw, inc, THEME, '.data')

    emit_generate_res_after('data')

def gen_res_all_flows():
    if not THEME_PACKAGED and THEME != 'default':
        return

    raw = join_path(INPUT_DIR, 'flows')
    if not os.path.exists(raw):
        return

    emit_generate_res_before('flows')

    if IS_GENERATE_RAW:
        if INPUT_DIR != join_path(OUTPUT_DIR, 'raw'):
            dst = join_path(OUTPUT_DIR, 'raw/flows')
            if os.path.exists(dst):
                remove_dir(dst)
            copy_dir(raw, dst)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/flows')
        make_dirs(inc)
        resgen(raw, inc, THEME, '.flows')

    emit_generate_res_after('flows')


def gen_res_all_xml():
    if not THEME_PACKAGED and THEME != 'default':
        return

    raw = join_path(INPUT_DIR, 'xml')
    if not os.path.exists(raw):
        return

    emit_generate_res_before('xml')

    if IS_GENERATE_RAW:
        if INPUT_DIR != join_path(OUTPUT_DIR, 'raw'):
            dst = join_path(OUTPUT_DIR, 'raw/xml')
            if os.path.exists(dst):
                remove_dir(dst)
            copy_dir(raw, dst)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/xml')
        make_dirs(inc)
        resgen(raw, inc, THEME, '.data')

    emit_generate_res_after('xml')


def gen_res_bitmap_font(input_dir, font_options, theme):
    if not os.path.exists(join_path(input_dir, 'fonts/config')):
        return

    for f in glob.glob(join_path(input_dir, 'fonts/config/*.txt')):
        filename, extname = os.path.splitext(f)
        fontname = os.path.basename(filename)
        index = fontname.rfind('_')
        if index > 0:
            raw = os.path.dirname(os.path.dirname(filename)) + '/origin/' + fontname[0 : index] + '.ttf'
            if not os.path.exists(raw):
                raw = os.path.dirname(os.path.dirname(filename)) + '/' + fontname[0 : index] + '.ttf'
            if os.path.exists(raw):
                size = fontname[index + 1 : len(fontname)]
                inc = join_path(OUTPUT_DIR, 'inc/fonts/' + fontname[0 : index] + '_' + str(size) + '.data')
                fontgen(raw, f, inc, size, font_options, theme)


def gen_res_all_font():
    if not THEME_PACKAGED and THEME != 'default':
        return

    emit_generate_res_before('font')

    if IS_GENERATE_RAW:
        if INPUT_DIR != join_path(OUTPUT_DIR, 'raw'):
            make_dirs(join_path(OUTPUT_DIR, 'raw/fonts'))

            in_files = join_path(INPUT_DIR, 'fonts/')
            for f in get_appint_folder(in_files, '.ttf', False):
                out_files = f[0].replace(INPUT_DIR, join_path(OUTPUT_DIR, 'raw'))
                out_foler = os.path.dirname(out_files)
                if not os.path.exists(out_foler):
                    make_dirs(out_foler)
                copy_file(f[0], out_files)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc_dir = join_path(OUTPUT_DIR, 'inc/fonts')
        make_dirs(inc_dir)

        if IS_GENERATE_INC_BITMAP:
            font_options = 'none'
            if IMAGEGEN_OPTIONS == 'mono':
                font_options = 'mono'

            # 位图字体的保留设置在fonts/config目录中，比如default_18.txt中设置default字体18字号的保留字符
            gen_res_bitmap_font(INPUT_DIR, font_options, THEME)
            if THEME != 'default':
                # 如果当前主题的gen选项与default主题不同时为mono，则按新的gen选项重新生成位图字体
                opt = get_imagegen_options_of_default_theme()
                if (opt == 'mono' or IMAGEGEN_OPTIONS == 'mono') and opt != IMAGEGEN_OPTIONS:
                    input_dir = join_path(ASSETS_ROOT, 'default/raw')
                    if not os.path.exists(input_dir):
                        input_dir = join_path(ASSETS_ROOT, 'default')
                    gen_res_bitmap_font(input_dir, font_options, THEME)

            # 对default_full的特殊处理，兼容awtk的demos
            IS_AWTK_DEMO = False
            if os.path.exists('scripts/update_res_common.py'):
                IS_AWTK_DEMO = True

            raw = join_path(INPUT_DIR, 'fonts/default_full.ttf')
            if IS_AWTK_DEMO and os.path.exists(raw):
                text = join_path(INPUT_DIR, 'fonts/text.txt')
                if os.path.exists(text):
                    fontsizes = [16, 18, 20, 24, 32, 96]
                    for size in fontsizes:
                        inc = join_path(OUTPUT_DIR, 'inc/fonts/default_%d.data' % size)
                        fontgen(raw, text, inc, size, font_options, THEME)

        if IS_GENERATE_INC_RES:
            if not os.path.exists(join_path(INPUT_DIR, 'fonts')):
                return

            for f in get_appint_folder(join_path(INPUT_DIR, 'fonts/'), ".ttf", False):
                filename, extname = os.path.splitext(f[0])
                raw = f[0]
                filename = filename.replace(INPUT_DIR, join_path(OUTPUT_DIR, 'inc'))
                inc = filename + '.res'
                resgen(raw, inc, THEME, '.res')

    emit_generate_res_after('font')


def gen_res_all_script():
    if not THEME_PACKAGED and THEME != 'default':
        return

    raw = join_path(INPUT_DIR, 'scripts')
    if not os.path.exists(raw):
        return

    emit_generate_res_before('script')

    if IS_GENERATE_RAW:
        if INPUT_DIR != join_path(OUTPUT_DIR, 'raw'):
            dst = join_path(OUTPUT_DIR, 'raw/scripts')
            if os.path.exists(dst):
                remove_dir(dst)
            copy_dir(raw, dst)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/scripts')
        make_dirs(inc)
        resgen(raw, inc, THEME, '.res')

    emit_generate_res_after('script')


def gen_res_all_string():
    if not THEME_PACKAGED and THEME != 'default':
        return

    raw = join_path(INPUT_DIR, 'strings/strings.xml')
    if not os.path.exists(raw):
        return

    emit_generate_res_before('string')

    if IS_GENERATE_RAW:
        bin = join_path(OUTPUT_DIR, 'raw/strings')
        make_dirs(bin)
        strgen_bin(raw, bin)

    if IS_GENERATE_INC_RES or IS_GENERATE_INC_BITMAP:
        inc = join_path(OUTPUT_DIR, 'inc/strings')
        make_dirs(inc)
        strgen(raw, inc, THEME)

    emit_generate_res_after('string')


def gen_gpinyin():
    emit_generate_res_before('gpinyin')
    exec_cmd('\"' + to_exe('resgen') + '\" \"' + join_path('3rd', 'gpinyin/data/gpinyin.dat') +
            '\" \"' + join_path('3rd', 'gpinyin/src/gpinyin.inc') + '\"')
    exec_cmd('\"' + to_exe('resgen') + '\" ' + join_path('tools', 'word_gen/words.bin') +
            '\" \"' + join_path('src', 'input_methods/suggest_words.inc') + '\"')
    exec_cmd('\"' + to_exe('resgen') + '\" \"' + join_path('tools','word_gen/words.bin') +
            '\" \"' + join_path('tests', 'suggest_test.inc') + '\"')
    emit_generate_res_after('gpinyin')


def gen_res_all():
    print('=========================================================')
    emit_generate_res_before('all')
    gen_res_all_string()
    gen_res_all_font()
    gen_res_all_script()
    gen_res_all_image()
    gen_res_all_ui()
    gen_res_all_style()
    gen_res_all_data()
    gen_res_all_flows()
    gen_res_all_xml()
    emit_generate_res_after('all')
    print('=========================================================')


def gen_includes(files, inherited, with_multi_theme):
    result = ""

    if with_multi_theme:
        remove = OUTPUT_ROOT
    else:
        remove = os.path.dirname(OUTPUT_ROOT)

    for f in files:
        if is_excluded_file(f):
            continue

        if inherited:
            filename = f.replace(join_path(OUTPUT_ROOT, 'default'), join_path(OUTPUT_ROOT, THEME))
            if os.path.exists(filename):
                continue

        incf = copy.copy(f)
        incf = incf.replace(remove, ".")
        incf = incf.replace('\\', '/')
        incf = incf.replace('./', '')
        result += '#include "'+incf+'"\n'

    return result


def gen_extern_declares(files):
    result = ""
    for f in files:
        if is_excluded_file(f):
            continue

        filename = f.replace(join_path(OUTPUT_ROOT, 'default'), join_path(OUTPUT_ROOT, THEME))
        if not os.path.exists(filename):
            constname = to_asset_const_name(f, join_path(OUTPUT_ROOT, 'default/inc'))
            result += 'extern TK_CONST_DATA_ALIGN(const unsigned char '+constname+'[]);\n'

    return result


def gen_assets_includes(curr_theme_path, defl_theme_path = None, with_multi_theme = True):
    files = glob_asset_files(curr_theme_path)
    result = gen_includes(files, False, with_multi_theme)

    if defl_theme_path != None and THEME != 'default':
        files = glob_asset_files(defl_theme_path)
        if with_multi_theme and is_packaged_of_default_theme():
            result += gen_extern_declares(files)
        else:
            result += gen_includes(files, True, with_multi_theme)

    return result


def gen_assets_add(filename, is_default_theme):
    if is_default_theme:
        constname = to_asset_const_name(filename, join_path(OUTPUT_ROOT, 'default/inc'))
        return '  assets_manager_add(am, '+constname+');\n'
    else:
        constname = to_asset_const_name(filename, join_path(OUTPUT_DIR, 'inc'))
        return '  assets_manager_add(am, '+constname+'_'+THEME+');\n'


def gen_assets_adds(curr_theme_path, defl_theme_path = None):
    result = ""
    is_default_theme = THEME == 'default'

    files = glob_asset_files(curr_theme_path)

    for f in files:
        if is_excluded_file(f):
            continue
        result += gen_assets_add(f, is_default_theme)
    if defl_theme_path != None and not is_default_theme:
        files = glob_asset_files(defl_theme_path)
        for f in files:
            if is_excluded_file(f):
                continue

            filename = f.replace(join_path(OUTPUT_ROOT, 'default'), join_path(OUTPUT_ROOT, THEME))
            if not os.path.exists(filename):
                result += gen_assets_add(f, True)

    return result


def gen_assets_c_of_one_theme(with_multi_theme = True):
    if not THEME_PACKAGED:
        return

    if with_multi_theme:
        filename = join_path(OUTPUT_ROOT, ASSETS_SUBNAME+THEME+'.inc')
    else:
        filename, extname = os.path.splitext(ASSET_C)
        filename = filename + '_' + THEME + extname

    func_name = 'assets_init'
    if with_multi_theme:
        func_name = 'assets_init_'+THEME

    result = '#include "awtk.h"\n'
    result += '#include "base/assets_manager.h"\n'
    result += '#if !defined(WITH_FS_RES) || defined(AWTK_WEB)\n'

    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/strings/*.data'), join_path(OUTPUT_ROOT, 'default/inc/strings/*.data'), with_multi_theme)
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/styles/*.data'), join_path(OUTPUT_ROOT, 'default/inc/styles/*.data'), with_multi_theme)
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/ui/*.data'), join_path(OUTPUT_ROOT, 'default/inc/ui/*.data'), with_multi_theme)
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/xml/*.data'), join_path(OUTPUT_ROOT, 'default/inc/xml/*.data'), with_multi_theme)
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/data/*.data'), join_path(OUTPUT_ROOT, 'default/inc/data/*.data'), with_multi_theme)
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/flows/*.flows'), join_path(OUTPUT_ROOT, 'default/inc/flows/*.flows'), with_multi_theme)
    result += "#ifndef AWTK_WEB\n"
    result += "#ifdef WITH_STB_IMAGE\n"
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/images/*.res'), join_path(OUTPUT_ROOT, 'default/inc/images/*.res'), with_multi_theme)
    result += "#else /*WITH_STB_IMAGE*/\n"
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/images/*.data'), join_path(OUTPUT_ROOT, 'default/inc/images/*.data'), with_multi_theme)
    result += '#endif /*WITH_STB_IMAGE*/\n'
    result += "#ifdef WITH_TRUETYPE_FONT\n"
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/fonts/*.res'), join_path(OUTPUT_ROOT, 'default/inc/fonts/*.res'), with_multi_theme)
    result += "#else /*WITH_TRUETYPE_FONT*/\n"
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/fonts/*.data'), join_path(OUTPUT_ROOT, 'default/inc/fonts/*.data'), with_multi_theme)
    result += '#endif /*WITH_TRUETYPE_FONT*/\n'
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/scripts/*.res'), join_path(OUTPUT_ROOT, 'default/inc/scripts/*.res'), with_multi_theme)
    result += '#endif /*AWTK_WEB*/\n'
    result += "#ifdef WITH_VGCANVAS\n"
    result += gen_assets_includes(join_path(OUTPUT_DIR, 'inc/images/*.bsvg'), join_path(OUTPUT_ROOT, 'default/inc/images/*.bsvg'), with_multi_theme)
    result += '#endif /*WITH_VGCANVAS*/\n'
    result += '#endif /*!defined(WITH_FS_RES) || defined(AWTK_WEB)*/\n'
    result += '\n'

    result += 'ret_t ' + func_name + '(void) {\n'
    result += '  assets_manager_t* am = assets_manager();\n'
    result += '  assets_manager_set_theme(am, "' + THEME + '");\n'
    result += '\n'

    result += '#if defined(WITH_FS_RES) && !defined(AWTK_WEB)\n'
    result += '  assets_manager_preload(am, ASSET_TYPE_STYLE, "default");\n'
    result += '#else /*defined(WITH_FS_RES) && !defined(AWTK_WEB)*/\n'
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/strings/*.data'), join_path(OUTPUT_ROOT, 'default/inc/strings/*.data'))
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/styles/*.data'), join_path(OUTPUT_ROOT, 'default/inc/styles/*.data'))
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/ui/*.data'), join_path(OUTPUT_ROOT, 'default/inc/ui/*.data'))
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/xml/*.data'), join_path(OUTPUT_ROOT, 'default/inc/xml/*.data'))
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/data/*.data'), join_path(OUTPUT_ROOT, 'default/inc/data/*.data'))
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/flows/*.flows'), join_path(OUTPUT_ROOT, 'default/inc/flows/*.flows'))
    result += '#ifndef AWTK_WEB\n'
    if IS_GENERATE_INC_RES:
        result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/images/*.res'), join_path(OUTPUT_ROOT, 'default/inc/images/*.res'))
    else:
        result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/images/*.data'), join_path(OUTPUT_ROOT, 'default/inc/images/*.data'))
    result += "#ifdef WITH_TRUETYPE_FONT\n"
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/fonts/*.res'), join_path(OUTPUT_ROOT, 'default/inc/fonts/*.res'))
    result += "#else /*WITH_TRUETYPE_FONT*/\n"
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/fonts/*.data'), join_path(OUTPUT_ROOT, 'default/inc/fonts/*.data'))
    result += '#endif /*WITH_TRUETYPE_FONT*/\n'
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/scripts/*.res'), join_path(OUTPUT_ROOT, 'default/inc/scripts/*.res'))
    result += '#endif /*AWTK_WEB*/\n'
    result += "#ifdef WITH_VGCANVAS\n"
    result += gen_assets_adds(join_path(OUTPUT_DIR, 'inc/images/*.bsvg'), join_path(OUTPUT_ROOT, 'default/inc/images/*.bsvg'))
    result += '#endif /*WITH_VGCANVAS*/\n'
    result += '#endif /*defined(WITH_FS_RES) && !defined(AWTK_WEB)*/\n'
    result += '\n'

    result += '  tk_init_assets();\n'
    result += '  return RET_OK;\n'
    result += '}\n'

    write_file(filename, result)


def gen_asset_c_entry_with_multi_theme():
    result = '#include "awtk.h"\n'
    result += '#include "base/assets_manager.h"\n'

    assets_root = os.path.relpath(OUTPUT_ROOT, os.path.dirname(ASSET_C)).replace('\\', '/')
    for i in range(len(THEMES)):
        set_current_theme(i)
        if THEME_PACKAGED:
            result += '#include "'+assets_root+'/'+ASSETS_SUBNAME+THEME+'.inc"\n'

    result += '\n'
    result += '#ifndef APP_THEME\n'
    result += '#define APP_THEME "' + APP_THEME + '"\n'
    result += '#endif /*APP_THEME*/\n\n'

    result += 'bool_t assets_has_theme(const char* name) {\n'
    result += '  return_value_if_fail(name != NULL, FALSE);\n\n'
    result += '  '
    for i in range(len(THEMES)):
        set_current_theme(i)
        if THEME_PACKAGED:
            result +=   'if (tk_str_eq(name, "'+THEME+'")) {\n'
            result += '    return TRUE;\n'
            result += '  } else '
    result += '{\n'
    result += '    return FALSE;\n  }\n}\n\n'

    result += 'static ret_t assets_init_internal(const char* theme) {\n'
    result += '  assets_manager_t* am = assets_manager();\n'
    result += '  return_value_if_fail(theme != NULL && am != NULL, RET_BAD_PARAMS);\n\n'
    result += '  assets_manager_set_theme(am, theme);\n\n'
    result += '  '
    for i in range(len(THEMES)):
        set_current_theme(i)
        if THEME_PACKAGED:
            result +=   'if (tk_str_eq(theme, "'+THEME+'")) {\n'
            result += '    return assets_init_'+THEME+'();\n'
            result += '  } else '
    result += '{\n'
    result += '    log_debug(\"%s not support.\\n\", theme);\n'
    result += '    return RET_NOT_IMPL;\n  }\n}\n\n'

    result += '#if !defined(WITH_FS_RES) || defined(AWTK_WEB)\n'
    result += 'static ret_t widget_set_theme_without_file_system(widget_t* widget, const char* name) {\n'
    result += '  const asset_info_t* info = NULL;\n'
    result += '  event_t e = event_init(EVT_THEME_CHANGED, NULL);\n'
    result += '  widget_t* wm = widget_get_window_manager(widget);\n'
    result += '  font_manager_t* fm = widget_get_font_manager(widget);\n'
    result += '  image_manager_t* imm = widget_get_image_manager(widget);\n'
    result += '  assets_manager_t* am = widget_get_assets_manager(widget);\n'
    result += '  locale_info_t* locale_info = widget_get_locale_info(widget);\n\n'
    result += '  return_value_if_fail(am != NULL && name != NULL, RET_BAD_PARAMS);\n'
    result += '  return_value_if_fail(assets_has_theme(name), RET_BAD_PARAMS);\n\n'
    result += '  font_manager_unload_all(fm);\n'
    result += '  image_manager_unload_all(imm);\n'
    result += '  assets_manager_clear_all(am);\n'
    result += '  widget_reset_canvas(widget);\n\n'
    result += '  assets_init_internal(name);\n'
    result += '  locale_info_reload(locale_info);\n\n'
    result += '  info = assets_manager_ref(am, ASSET_TYPE_STYLE, "default");\n'
    result += '  theme_set_theme_data(theme(), info->data);\n'
    result += '  assets_manager_unref(assets_manager(), info);\n\n'
    result += '  widget_dispatch(wm, &e);\n'
    result += '  widget_invalidate_force(wm, NULL);\n\n'
    result += '  log_debug("theme changed: %s\\n", name);\n\n'
    result += '  return RET_OK;\n'
    result += '}\n\n'
    result += 'static ret_t on_set_theme_without_file_system(void* ctx, event_t* e) {\n'
    result += '  theme_change_event_t* evt = theme_change_event_cast(e);\n'
    result += '  widget_set_theme_without_file_system(window_manager(), evt->name);\n'
    result += '  return RET_OK;\n'
    result += '}\n'
    result += '#endif /*!defined(WITH_FS_RES) || defined(AWTK_WEB)*/\n\n'

    result += 'ret_t assets_init(void) {\n'
    result += '#if !defined(WITH_FS_RES) || defined(AWTK_WEB)\n'
    result += '  widget_on(window_manager(), EVT_THEME_WILL_CHANGE, on_set_theme_without_file_system, NULL);\n'
    result += '#endif /*!defined(WITH_FS_RES) || defined(AWTK_WEB)*/\n'
    result += '  return assets_init_internal(APP_THEME);\n}\n\n'

    result += 'ret_t assets_set_global_theme(const char* name) {\n'
    result += '  return widget_set_theme(window_manager(), name);\n'
    result += '}\n'

    write_file(ASSET_C, result)

def gen_res_c(with_multi_theme = True):
    theme_foreach(gen_assets_c_of_one_theme, with_multi_theme)

    if with_multi_theme:
        gen_asset_c_entry_with_multi_theme()

def gen_res_json_one(res_type, files):
    result = ''
    for f in files:
        uri = f.replace(os.getcwd(), "")[1:]
        uri = uri.replace('\\', '/')
        filename, extname = os.path.splitext(uri)
        basename = os.path.basename(filename)
        result = result + '    {name:"' + basename + '\", uri:"' + uri
        if res_type == 'image' and extname != '.svg' and extname != '.bsvg':
            from PIL import Image
            img = Image.open(f)
            w, h = img.size
            result = result + '", w:' + str(w) + ', h:' + str(h) + '},\n'
        else:
            result = result + '"},\n'

    return result

def gen_res_json_all_theme(res_type, sub_filepath):
    result = "\n  " + res_type + ': [\n'
    for i in range(len(THEMES)):
        set_current_theme(i)
        files = glob.glob(join_path(INPUT_DIR, sub_filepath))
        result += gen_res_json_one(res_type, files)
    result += '  ],'

    return result

def gen_res_json():
    result = 'const g_awtk_assets = {'
    result += gen_res_json_all_theme("image", 'images/*/*.*')
    result += gen_res_json_all_theme("ui", 'ui/*.bin')
    result += gen_res_json_all_theme("style",'styles/*.bin')
    result += gen_res_json_all_theme("string", 'strings/*.bin')
    result += gen_res_json_all_theme("xml", 'xml/*.xml')
    result += gen_res_json_all_theme("data", 'data/*.*')
    result += gen_res_json_all_theme("script", 'scripts/*.*')
    result += gen_res_json_all_theme("font", 'fonts/*.ttf')
    result += '\n};\n'

    write_file(ASSET_C.replace('.c', '_web.js'), result)


def init(awtk_root, assets_root, themes, asset_c, output = None):
    global THEMES
    global ASSET_C
    global BIN_DIR
    global ASSETS_ROOT
    global AWTK_ROOT
    global OUTPUT_ROOT

    ASSET_C = asset_c
    AWTK_ROOT = awtk_root
    ASSETS_ROOT = assets_root
    BIN_DIR = join_path(AWTK_ROOT, 'bin')

    if output != None:
        OUTPUT_ROOT = output
    else:
        OUTPUT_ROOT = ASSETS_ROOT

    if isinstance(themes, str):
        THEMES = [themes]
    else:
        THEMES = themes

    set_current_theme(0)


def dump_args():
    print('---------------------------------------------------------')
    print('DPI = '+DPI)
    print('THEMES = '+str(THEMES))
    print('IMAGEGEN_OPTIONS = '+IMAGEGEN_OPTIONS)
    print('LCD_FAST_ROTATION_MODE = '+str(LCD_FAST_ROTATION_MODE))
    print('LCD_ORIENTATION = LCD_ORIENTATION_'+LCD_ORIENTATION)
    print('AWTK_ROOT = '+AWTK_ROOT)
    print('BIN_DIR = '+BIN_DIR)
    print('ASSETS_ROOT = '+ASSETS_ROOT)
    print('ASSET_C = '+ASSET_C)
    print('OUTPUT = '+OUTPUT_ROOT)


def update_res():
    if ACTION == 'all':
        clean_res()
        theme_foreach(gen_res_all)
        gen_res_c()
    elif ACTION == 'res':
        clean_res()
        theme_foreach(gen_res_all)
    elif ACTION == 'clean':
        clean_res()
    elif ACTION == 'web':
        gen_res_c()
    elif ACTION == 'json':
        gen_res_json()
    elif ACTION == 'string':
        theme_foreach(gen_res_all_string)
        gen_res_c()
    elif ACTION == "font":
        theme_foreach(gen_res_all_font)
        gen_res_c()
    elif ACTION == "script":
        theme_foreach(gen_res_all_script)
        gen_res_c()
    elif ACTION == 'image':
        theme_foreach(gen_res_all_image)
        gen_res_c()
    elif ACTION == 'ui':
        theme_foreach(gen_res_all_ui)
        gen_res_c()
    elif ACTION == 'style':
        theme_foreach(gen_res_all_style)
        gen_res_c()
    elif ACTION == 'data':
        theme_foreach(gen_res_all_data)
        gen_res_c()
    elif ACTION == 'flows':
        theme_foreach(gen_res_all_flows)
        gen_res_c()
    elif ACTION == 'xml':
        theme_foreach(gen_res_all_xml)
        gen_res_c()
    elif ACTION == 'pinyin':
        gen_gpinyin()
    elif ACTION == 'assets.c':
        gen_res_c()

    dump_args()


def clean_res_bin(dir):
    if os.path.isdir(dir):
        for root, dirs, files in os.walk(dir):
            for f in files:
                filename, extname = os.path.splitext(f)
                if extname == '.bin' or extname == '.bsvg':
                    real_path = join_path(root, f)
                    os.chmod(real_path, stat.S_IRWXU)
                    os.remove(real_path)


def clean_res_of_one_theme():
    print('clean res: ' + OUTPUT_DIR)
    clean_res_bin(join_path(OUTPUT_DIR, 'raw/ui'))
    clean_res_bin(join_path(OUTPUT_DIR, 'raw/images/svg'))
    clean_res_bin(join_path(OUTPUT_DIR, 'raw/strings'))
    clean_res_bin(join_path(OUTPUT_DIR, 'raw/styles'))
    remove_dir(join_path(OUTPUT_DIR, 'inc'))
    remove_dir(join_path(OUTPUT_ROOT, ASSETS_SUBNAME+THEME+'.inc'))

def clean_res():
    print('=========================================================')
    if ASSETS_ROOT != OUTPUT_ROOT:
        dir = os.path.dirname(OUTPUT_ROOT)
        print('clean res: ' + dir)
        remove_dir(dir)
    else:
        theme_foreach(clean_res_of_one_theme)
        remove_dir(ASSET_C)
    print('=========================================================')

def get_args(args) :
    list_args = []
    for arg in args:
        if arg.startswith('--') :
            continue
        list_args.append(arg)
    return list_args

def get_opts(args) :
    import getopt
    longsots = ['awtk_root=', 'AWTK_ROOT=', 
                'action=', 'ACTION=',
                'help', 'HELP',
                'dpi=', 'DPI=', 
                'image_options=', 'IMAGE_OPTIONS=', 
                'lcd_orientation=', 'LCD_ORIENTATION=', 
                'lcd_enable_fast_rotation=', 'LCD_ENABLE_FAST_ROTATION=',
                'res_config_file=', 'RES_CONFIG_FILE=',
                'res_config_script=', 'RES_CONFIG_SCRIPT=',
                'res_config_script_argv=', 'RES_CONFIG_SCRIPT_ARGV=',
                'output_dir=', 'OUTPUT_DIR=',
                'app_root=', 'APP_ROOT',
                ];
    try :
        opts, tmp_args = getopt.getopt(args, 'h', longsots);
        return opts;
    except getopt.GetoptError as err:
        print(err.msg);
    return None;

def is_all_sopts_args(args) :
    for arg in args:
        if not arg.startswith('--') and not arg.startswith('-'):
            return False;
    return True;

def get_longsopt_name_by_tuple(tuple) :
    return tuple[0][2:].lower();

def get_shortopt_name_by_tuple(tuple) :
    return tuple[0][1:].lower();

def get_longsopts_args(args) :
    opts = get_opts(args);
    if opts != None :
        data = {};
        for tmp_opts in opts :
            value = tmp_opts[1]
            if isinstance(value, str) and value[0] == '"' and value[-1] == '"' :
                value = value[1:-1]
            data[get_longsopt_name_by_tuple(tmp_opts)] = value
        return data;
    else :
        return None;

def show_usage_imlp(is_new_usage):
    if is_new_usage :
        print('=========================================================')
        print('Usage: python '+sys.argv[0])
        print('--action :'.ljust(30) + ' update res action, this sopt must set ')
        print(''.ljust(30) + ' use : --action=clean|web|json|all|font|image|ui|style|string|script|data|xml|assets.c')
        print(''.ljust(30) + ' clean'.ljust(10) +': clear res')
        print(''.ljust(30) + ' all'.ljust(10) +': update all res')
        print(''.ljust(30) + ' web'.ljust(10) +': update web res')
        print(''.ljust(30) + ' json'.ljust(10) +': only update json res')
        print(''.ljust(30) + ' font'.ljust(10) +': only update font res')
        print(''.ljust(30) + ' image'.ljust(10) +': only update image res')
        print(''.ljust(30) + ' ui'.ljust(10) +': only update ui res')
        print(''.ljust(30) + ' style'.ljust(10) +': only update style res')
        print(''.ljust(30) + ' string'.ljust(10) +': only update translator string res')
        print(''.ljust(30) + ' script'.ljust(10) +': only update script res')
        print(''.ljust(30) + ' data'.ljust(10) +': only update data res')
        print(''.ljust(30) + ' xml'.ljust(10) +': only update xml res')
        print(''.ljust(30) + ' assets.c'.ljust(10) +': only update assets.c file')

        print('--awtk_root :'.ljust(30) + ' set awtk root')
        print(''.ljust(30) + ' use : --awtk_root=XXXXX')

        print('--dpi :'.ljust(30) + ' set update res dpi')
        print(''.ljust(30) + ' use : --dpi=x1|x2|x3')

        print('--image_options :'.ljust(30) + ' set image foramt')
        print(''.ljust(30) + ' use : --image_options=rgba|bgra+bgr565|mono')

        print('--res_config_file :'.ljust(30) + ' set res config file path, priority : res_config_script > res_config_file')
        print(''.ljust(30) + ' use : --res_config_file=XXXXX')

        print('--res_config_script :'.ljust(30) + ' set res config script file path, this is script must has get_res_config(argv) function ')
        print(''.ljust(30) + ' use : --res_config_script=XXXXX')

        print('--res_config_script_argv :'.ljust(30) + ' set res config script argv, this is get_res_config() function parameter')
        print(''.ljust(30) + ' use : --res_config_script_argv=XXXXX')

        print('--lcd_orientation :'.ljust(30) + ' set lcd orientation ')
        print(''.ljust(30) + ' use : --lcd_orientation=90/180/270')

        print('--lcd_enable_fast_rotation :'.ljust(30) + ' set enable lcd fast rotation ')
        print(''.ljust(30) + ' use : --lcd_enable_fast_rotation=true/false')

        print('--output_dir :'.ljust(30) + ' set res output dir, default value is ./res ')
        print(''.ljust(30) + ' use : --output_dir=XXXXX')

        print('--app_root :'.ljust(30) + ' set app root dir, default value is getcwd() ')
        print(''.ljust(30) + ' use : --app_root=XXXXX')

        print('--help :'.ljust(30) + ' show all usage ')
        print(''.ljust(30) + ' use : --help')

        print('---------------------------------------------------------')
        print('Example:')
        print('python ' + sys.argv[0] + ' --action=all')
        print('python ' + sys.argv[0] + ' --action=clean')
        print('python ' + sys.argv[0] + ' --action=style --awtk_root=XXXXX ')
        print('python ' + sys.argv[0] + ' --action=all --dpi=x1 --image_options=bgra+bgr565')
        print('python ' + sys.argv[0] + ' --action=all --dpi=x1 --image_options=bgra+bgr565 --awtk_root=XXXXX')
        print('=========================================================')
    else :
        args = ' action[clean|web|json|all|font|image|ui|style|string|script|data|xml|assets.c] dpi[x1|x2] image_options[rgba|bgra+bgr565|mono] awtk_root[--awtk_root=XXXXX]'
        print('=========================================================')
        print('Usage: python '+sys.argv[0] + args)
        print('Example:')
        print('python ' + sys.argv[0] + ' all')
        print('python ' + sys.argv[0] + ' clean')
        print('python ' + sys.argv[0] + ' style --awtk_root=XXXXX ')
        print('python ' + sys.argv[0] + ' all x1 bgra+bgr565')
        print('python ' + sys.argv[0] + ' all x1 bgra+bgr565 --awtk_root=XXXXX')
        print('=========================================================')
    if exit :
        sys.exit(0)

def show_usage(is_new_usage = False):
    if len(sys.argv) == 1:
        show_usage_imlp(is_new_usage)
    else:
        args = sys.argv[1:];
        if is_all_sopts_args(args) :
            opts = get_opts(args);
            if opts == None :
                show_usage_imlp(is_new_usage)
            else :
                find_action = False;
                for tmp_opts in opts :
                    str_opt = get_longsopt_name_by_tuple(tmp_opts);
                    if str_opt == 'help' or get_shortopt_name_by_tuple(tmp_opts) == 'h':
                        show_usage_imlp(is_new_usage)
                    elif str_opt == 'action' :
                        find_action = True;
                if not find_action :
                    show_usage_imlp(is_new_usage)
        else :
            sys_args = get_args(args)
            if len(sys_args) == 0 :
                show_usage_imlp(is_new_usage)

