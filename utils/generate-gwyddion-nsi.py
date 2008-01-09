import os, tempfile

GTK_PATH = "../gtk-mini/"
GWYDDION_PATH = "inst/"
GWYDDION_CONFIG_H = "config.h"
GWYDDION_NSI_TEMPLATE = "utils/gwyddion.nsi.template"



# get version:
def get_version():
    config_file = file(GWYDDION_CONFIG_H, "r")
    l = config_file.readline()
    while l != "":  
        if l.startswith("#define PACKAGE_VERSION"):
            return l.split('"')[1]
        l = config_file.readline()
    print "Warning: Cannot determine version."
    return ""

ignore_dirs = ['devel-docs', 'gone'] # in lowercase
ignore_files = ['pygwy.dll'] # in lowercase

def list_of_files(arg, dirname, fnames):
    print dirname
    for c in dirname.split('\\'):      
        if c.lower() in ignore_dirs:        
            return    
    arg.append(dirname)
    for fname in fnames:    
        if fname.lower() in ignore_dirs:
            continue
        if fname.lower() in ignore_files:          
            continue            
        arg.append(create_path(dirname, fname))        

def create_path(dir, filename):
    if dir == "":
        return filename
    if dir.endswith('\\'):
        return dir + filename
    return dir + '\\' + filename

def get_install_line(filename, prefix):
    if os.path.isdir(filename):
        return '   CreateDirectory "$INSTDIR\\'+filename.replace(prefix, "", 1)+'"\n'
    else:
        return '   File "/oname='+filename.replace(prefix, "", 1)+'" "'+filename+'"\n'

def get_uninstall_line(filename, prefix):
    if os.path.isdir(filename):
        return '   RmDir "$INSTDIR\\'+filename.replace(prefix, "", 1)+'"\n'
    else:
        return '   Delete "$INSTDIR\\'+filename.replace(prefix, "", 1)+'"\n'
    
gtk_files = []
gwyddion_files = []

os.path.walk(GTK_PATH, list_of_files, gtk_files)
os.path.walk(GWYDDION_PATH, list_of_files, gwyddion_files)

#print gtk_files

gtk_install_files = ""
for f in gtk_files:
    gtk_install_files += get_install_line(f, GTK_PATH)

gtk_uninstall_files = ""
gtk_files.reverse()
for f in gtk_files:
    gtk_uninstall_files += get_uninstall_line(f, GTK_PATH)

gwyddion_install_files = ""
for f in gwyddion_files:
    gwyddion_install_files += get_install_line(f, GWYDDION_PATH)

gwyddion_uninstall_files = ""
gwyddion_files.reverse()
for f in gwyddion_files:
    gwyddion_uninstall_files += get_uninstall_line(f, GWYDDION_PATH)


nsi_template = file(GWYDDION_NSI_TEMPLATE ,"r")
content_nsi = nsi_template.read()
nsi_template.close()

content_nsi = content_nsi.replace("%VERSION%", get_version())

content_nsi = content_nsi.replace("%GWYDDION_INSTALL%", gwyddion_install_files)
content_nsi = content_nsi.replace("%GTK_INSTALL%", gtk_install_files)
content_nsi = content_nsi.replace("%GWYDDION_UNINSTALL%", gwyddion_uninstall_files)
content_nsi = content_nsi.replace("%GTK_UNINSTALL%", gtk_uninstall_files)
GWYDDION_PATH = "d:\\gwyddion-2.8\\inst\\"
content_nsi = content_nsi.replace("%PYGWY_INSTALL%", '   File "'+GWYDDION_PATH+'modules\\pygwy.dll"')
content_nsi = content_nsi.replace("%PYGWY_UNINSTALL%", '   Delete "$INSTDIR\\modules\\pygwy.dll"')

nsi = file("gwyddion.nsi", "w")
nsi.write(content_nsi)
nsi.close()
