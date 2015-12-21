#!/usr/bin/python
#coding: utf-8

# Generate Gwyddion Visual Studio Solution 
# Created for Visual Studio 2015

from __future__ import division
import sys, glob, os, time, xml.dom.minidom, subprocess, platform, uuid, shutil, string, argparse
from xml.etree.ElementTree import Element, SubElement

indent_string = '\t'

# projects
# key = (path, projname, uuid)  i.e. ('.\\gwyddion\\modules\\file', 'accurexii-txt', '5D8DCF07-3F84-4443-B5D8-306650040B10')
# value = [list]                list of .c, .h, .def files 
projs = {}

# key = (path)
# value = UUID
solution_folders = {} 

include_dirs_win32 = ['$(SolutionDir)..\\',
'$(GTK_DIR)Win32\\include', '$(GTK_DIR)Win32\\include\\atk-1.0', '$(GTK_DIR)Win32\\include\\cairo', 
'$(GTK_DIR)Win32\\include\\freetype2', '$(GTK_DIR)Win32\\include\\gdk-pixbuf-2.0',
'$(GTK_DIR)Win32\\include\\glib-2.0', '$(GTK_DIR)Win32\\include\\gtk-2.0', 
'$(GTK_DIR)Win32\\lib\\gtk-2.0\\include', '$(GTK_DIR)Win32\\lib\\glib-2.0\\include',
'$(GTK_DIR)Win32\\include\\pango-1.0', '$(GTKGLEXT_DIR)Win32\\include',
'$(CFITSIO_DIR)Win32', '$(LIBXML_DIR)include', '$(LIBICONV_DIR)include',
'$(PYTHON_DIR)Win32\\include', '$(PYTHON_DIR)Win32\\include\\pygtk-2.0', '$(ZLIB_DIR)contrib\\minizip', '$(IncludePath)']

include_dirs_x64 = ['$(SolutionDir)..\\',
'$(GTK_DIR)x64\\include', '$(GTK_DIR)x64\\include\\atk-1.0', '$(GTK_DIR)x64\\include\\cairo', 
'$(GTK_DIR)x64\\include\\freetype2', '$(GTK_DIR)x64\\include\\gdk-pixbuf-2.0',
'$(GTK_DIR)x64\\include\\glib-2.0', '$(GTK_DIR)x64\\include\\gtk-2.0', 
'$(GTK_DIR)x64\\lib\\gtk-2.0\\include', '$(GTK_DIR)x64\\lib\\glib-2.0\\include',
'$(GTK_DIR)x64\\include\\pango-1.0', '$(GTKGLEXT_DIR)x64\\include',
'$(CFITSIO_DIR)x64', '$(LIBXML_DIR)include', '$(LIBICONV_DIR)include',
'$(PYTHON_DIR)x64\\include', '$(PYTHON_DIR)x64\\include\\pygtk-2.0', '$(ZLIB_DIR)contrib\\minizip', '$(IncludePath)']

library_dirs_win32 = ['$(OutDir)', '$(GTK_DIR)Win32\\lib', '$(GTKGLEXT_DIR)Win32\\lib',
'$(PYTHON_DIR)Win32\\libs', '$(PYTHON_DIR)Win32\\libs\\site-packages\\gtk-2.0\\runtime\\lib', 
'$(CFITSIO_DIR)Win32', '$(LIBXML_DIR)\lib', '$(LIBICONV_DIR)include', '$(ZLIB_DIR)x86\lib', '$(LibraryPath)']

library_dirs_x64 = ['$(OutDir)', '$(GTK_DIR)x64\\lib', '$(GTKGLEXT_DIR)x64\\lib',
'$(PYTHON_DIR)x64\\libs', '$(PYTHON_DIR)x64\\libs\\site-packages\\gtk-2.0\\runtime\\lib', 
'$(CFITSIO_DIR)x64', '$(LIBXML_DIR)\lib', '$(LIBICONV_DIR)include', '$(ZLIB_DIR)x64\\lib', '$(LibraryPath)']

libs_win32 = ['cairo.lib', 'cfitsio.lib', 'gdk-win32-2.0.lib', 'gdk_pixbuf-2.0.lib', 
'gtk-win32-2.0.lib', 'gio-2.0.lib', 'glib-2.0.lib', 'gmodule-2.0.lib',
'gobject-2.0.lib', 'intl.lib', 'libgtkglext-win32-1.0.lib', 'libgdkglext-win32-1.0.lib',
'libpng.lib', 'libxml2.lib', 'OpenGL32.Lib', 'pango-1.0.lib', 'pangocairo-1.0.lib',
'pangoft2-1.0.lib', 'python27.lib', 'zlibstat.lib', 'zlib.lib', '%(AdditionalDependencies)']

libs_x64 = ['cairo.lib', 'cfitsio.lib', 'gdk-win32-2.0.lib', 'gdk_pixbuf-2.0.lib', 
'gtk-win32-2.0.lib', 'gio-2.0.lib', 'glib-2.0.lib', 'gmodule-2.0.lib',
'gobject-2.0.lib', 'intl.lib', 'libgtkglext-win32-1.0.lib', 'libgdkglext-win32-1.0.lib',
'libpng.lib', 'libxml2.lib', 'OpenGL32.Lib', 'pango-1.0.lib', 'pangocairo-1.0.lib',
'pangoft2-1.0.lib', 'python27.lib', 'zlibwapi.lib', '%(AdditionalDependencies)']

generated_files_by_autotools = [
'config.h', 'gwyconfig.h', 
'app//gwyapptypes.h', 'app//gwyapptypes.c', 'app//authors.h', 
'libgwyddion//gwyddiontypes.h', 'libgwyddion//gwyddiontypes.c', 'libgwyddion//gwyversion.h', 
'libgwydgets//gwydgettypes.h', 'libgwydgets//gwydgettypes.c', 
'libgwydgets//gwydgetmarshals.h', 'libgwydgets//gwydgetmarshals.c', 
'libgwymodule//gwymoduletypes.h', 'libgwymodule//gwymoduletypes.c', 
'libprocess//gwyprocesstypes.h', 'libprocess//gwyprocesstypes.c',
'modules//pygwy//pygwywrap.c']

# exclude conditional compilation projects
excluded_projects_win32 = ['apedaxfile', 'hdrimage', 'nanoobserver', 'nanoscantech', 'opengps', 'sensofarx', 'spmxfile']

excluded_projects_x64 = ['apedaxfile', 'hdrimage', 'nanoobserver', 'nanoscantech', 'opengps', 'sensofarx', 'spmxfile', 'spml']

post_build_event_files_to_copy_win32 = [
'xcopy /y /d "$(CFITSIO_DIR)Win32\\cfitsio.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\freetype6.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\intl.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libatk-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libcairo-2.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libexpat-1.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libfontconfig-1.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libgdk-win32-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libgdk_pixbuf-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libgtk-win32-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libgio-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libglib-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libgmodule-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libgobject-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libgthread-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libpango-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libpangocairo-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libpangoft2-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libpangowin32-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\libpng14-14.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)Win32\\bin\\zlib1.dll" "$(OutDir)"',
'xcopy /y /d "$(LIBICONV_DIR)\\bin\\iconv.dll" "$(OutDir)"',
'xcopy /y /d "$(LIBXML_DIR)bin\\libxml2.dll" "$(OutDir)"',
'xcopy /y /d "$(GTKGLEXT_DIR)Win32\\bin\\libgtkglext-win32-1.0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTKGLEXT_DIR)Win32\\bin\\libgdkglext-win32-1.0.dll" "$(OutDir)"',
'xcopy /y /d "$(SolutionDir)..\\data\\glmaterials\\*." "$(OutDir)share\\gwyddion\\glmaterials\\"',
'xcopy /y /d "$(SolutionDir)..\\data\\gradients\\*." "$(OutDir)share\\gwyddion\\gradients\\"',
'xcopy /y /d "$(SolutionDir)..\\data\\gradients\\Gwyddion.net" "$(OutDir)share\\gwyddion\\gradients\\"',
'xcopy /y /d "$(SolutionDir)..\\pixmaps\\*.ico" "$(OutDir)share\\gwyddion\\pixmaps\\"',
'xcopy /y /d "$(SolutionDir)..\\pixmaps\\*.png" "$(OutDir)share\\gwyddion\\pixmaps\\"',
'xcopy /y /d "$(SolutionDir)..\\app\\toolbox.xml" "$(OutDir)share\\gwyddion\\ui\\"',
'xcopy /y /d "$(SolutionDir)..\\utils\\user-guide-modules" "$(OutDir)share\\gwyddion\\"'
]

post_build_event_files_to_copy_x64 = [
'xcopy /y /d "$(CFITSIO_DIR)x64\\cfitsio.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\freetype6.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\intl.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libatk-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libcairo-2.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libexpat-1.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libfontconfig-1.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libfreetype-6.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libintl-8.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libgdk-win32-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libgdk_pixbuf-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libgtk-win32-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libgio-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libglib-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libgmodule-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libgobject-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libgthread-2.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libpango-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libpangocairo-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libpangoft2-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libpangowin32-1.0-0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\libpng14-14.dll" "$(OutDir)"',
'xcopy /y /d "$(GTK_DIR)x64\\bin\\zlib1.dll" "$(OutDir)"',
'xcopy /y /d "$(LIBICONV_DIR)\\bin\\iconv.dll" "$(OutDir)"',
'xcopy /y /d "$(LIBXML_DIR)bin\\libxml2.dll" "$(OutDir)"',
'xcopy /y /d "$(GTKGLEXT_DIR)x64\\bin\\libgtkglext-win32-1.0.dll" "$(OutDir)"',
'xcopy /y /d "$(GTKGLEXT_DIR)x64\\bin\\libgdkglext-win32-1.0.dll" "$(OutDir)"',
'xcopy /y /d "$(ZLIB_DIR)x64\\bin\\zlibwapi.dll" "$(OutDir)"',
'xcopy /y /d "$(SolutionDir)..\\data\\glmaterials\\*." "$(OutDir)share\\gwyddion\\glmaterials\\"',
'xcopy /y /d "$(SolutionDir)..\\data\\gradients\\*." "$(OutDir)share\\gwyddion\\gradients\\"',
'xcopy /y /d "$(SolutionDir)..\\data\\gradients\\Gwyddion.net" "$(OutDir)share\\gwyddion\\gradients\\"',
'xcopy /y /d "$(SolutionDir)..\\pixmaps\\*.ico" "$(OutDir)share\\gwyddion\\pixmaps\\"',
'xcopy /y /d "$(SolutionDir)..\\pixmaps\\*.png" "$(OutDir)share\\gwyddion\\pixmaps\\"',
'xcopy /y /d "$(SolutionDir)..\\app\\toolbox.xml" "$(OutDir)share\\gwyddion\\ui\\"',
'xcopy /y /d "$(SolutionDir)..\\utils\\user-guide-modules" "$(OutDir)share\\gwyddion\\"'
]

modules_copy_command_string = 'xcopy /y /d "$(OutDir)$(ProjectName).dll" "$(OutDir){0}"'


################################################################################
# Inserts gwyddion_root_folder to 'include directory lists' and 'post build event files to copy' 
def set_gwyddion_root_folder_to_global_lists(gwyddion_root_folder):    
    # set gwyddion root folder to include dirs 
    # i.e. '$(SolutionDir)..\\..\\$(GwyddionFolder)' --> '$(SolutionDir)..\\..\\gwyddion' 
    for x in include_dirs_win32:
        if x.find('$(GwyddionFolder)') != -1: 
            i = include_dirs_win32.index(x)
            x = x.replace('$(GwyddionFolder)', gwyddion_root_folder)            
            include_dirs_win32.pop(i)
            include_dirs_win32.insert(i, x)            
            
    for x in include_dirs_x64:
        if x.find('$(GwyddionFolder)') != -1: 
            i = include_dirs_x64.index(x)
            x = x.replace('$(GwyddionFolder)', gwyddion_root_folder)            
            include_dirs_x64.pop(i)
            include_dirs_x64.insert(i, x)
            
    # set gwyddion root folder to post build event command paths 
    # i.e. "$(SolutionDir)..\\..\\$(GwyddionFolder)\\pixmaps\\*.ico" --> "$(SolutionDir)..\\..\\gwyddion\\pixmaps\\*.ico" 
    for x in post_build_event_files_to_copy_win32:
        if x.find('$(GwyddionFolder)') != -1: 
            i = post_build_event_files_to_copy_win32.index(x)
            x = x.replace('$(GwyddionFolder)', gwyddion_root_folder)            
            post_build_event_files_to_copy_win32.pop(i)
            post_build_event_files_to_copy_win32.insert(i, x)            
    
    for x in post_build_event_files_to_copy_x64:
        if x.find('$(GwyddionFolder)') != -1: 
            i = post_build_event_files_to_copy_x64.index(x)
            x = x.replace('$(GwyddionFolder)', gwyddion_root_folder)            
            post_build_event_files_to_copy_x64.pop(i)
            post_build_event_files_to_copy_x64.insert(i, x)    
 

################################################################################
# Returns project name 
# If variable then expand it
# project_name - project (or variable to expand) name
# content       - content of makefile 
def expand_variable(project_name, content):
    if((project_name.startswith('$(') == False) and (project_name.endswith(')') == False)):
        return project_name
        
    variable_name = project_name.replace('$(', '')
    variable_name = variable_name.replace(')', '') 
    for line in content.split('\n'):
        if line.startswith('#'): continue
        spl = ''
        if (line.startswith(variable_name + ' =')):
            spl = line.split(variable_name + ' =')            
        if (line.startswith(variable_name + '=')):
            spl = line.split(variable_name + '=')
        if len(spl) == 2:                        
            result = spl[1].replace('=', '')
            result = result.replace('.la', '')
            result = result.strip()                        
            return result
            
    return '' 
            

################################################################################
# Parse makefile 
# root      - makefile path
# fullpath  - makefile filename including path
def parse_makefile(root, fullpath):
    content = [line.strip() for line in file(fullpath)]
    content = '\n'.join(content)
    content = content.replace('\\\n', ' ')
    projname = ''
    makefile_variables = {}
    makefile_variables_list = []
    for line in content.split('\n'):
        if line.startswith('#'): continue
        
        if (1):
            # correct way to get project name
            if '_LTLIBRARIES' in line:
                spl = line.split('_LTLIBRARIES')
                if len(spl) == 2:
                    items = spl[1].replace('=', '')
                    items = items.strip()
                    items = items.split()
                    
                    for item in items:
                        projname = item.replace('.la','')
                        projname = projname.replace('-', '_')
                                                  
                        projname = expand_variable(projname, content)                           
                        if (projname == ''): continue                                                
                                
                        makefile_variables_list = [  'noinst_HEADERS',
                                                     projname + 'include_HEADERS',                                                    
                                                     projname + '_la_SOURCES',
                                                     projname + '_la_DEPENDENCIES']
                                                     
                        makefile_variables[projname] = (makefile_variables_list, 'DynamicLibrary')
                        #makefile_variables[projname][0] = makefile_variables_list 
                        #makefile_variables[projname][1] = 'DynamicLibrary'
                    
                    #key = (root, projname)
                    #projs[key] = 'N//A'
                    
            if 'bin_PROGRAMS' in line:
                spl = line.split('bin_PROGRAMS')
                if len(spl) == 2:
                    items = spl[1].replace('=', '')
                    items = items.strip()
                    items = items.split()
                    projname = items[0]
                    
                    #print type(makefile_variables_list)
                    makefile_variables_list = [ 'noinst_HEADERS',
                                                 projname + '_SOURCES']
                    
                    makefile_variables[projname] = (makefile_variables_list, 'Application')                             
                    #makefile_variables[projname][0] = makefile_variables_list
                    #makefile_variables[projname][1] = 'Application'                                                                                    
                                                
                    #key = (root, projname)
                    #projs[key] = 'N//A'                                                                                                                                                                                                                                      

        if (0):
            # easier way to get project name
            spl = line.split('_la_SOURCES')
            if (len(spl) == 2):
                projname = spl[0]
                key = (root, projname)
                items = spl[1].replace('=', '')
                items = items.strip()
                items = items.split()
                if key in projs:
                    projs[key][0].extend(items)
                else:
                    projs[key][0] = items 
            else:                                               
                if ('_SOURCES' in line):
                    spl = line.split('_SOURCES')
                    if (len(spl) == 2):
                        projname = spl[0]
                        key = (root, projname)
                        items = spl[1].replace('=', '')
                        items = items.strip()
                        items = items.split()
                        if key in projs:
                            projs[key][0].extend(items)
                        else:
                            projs[key][0] = items
            
            if (0):
                if ('include_HEADERS' in line):
                    #if line.startswith('noinst_HEADERS'): continue
                    spl = line.split('include_HEADERS')
    
                if ('_HEADERS' in line):
                    spl = line.split('_HEADERS')
                    if len(spl) == 2:
                        projname = spl[0]
                        key = (root, projname)
                        items = spl[1].replace('=', '')
                        items = items.strip()
                        items = items.split()
                        if key in projs:
                            projs[key].extend(items)
                        else:
                            projs[key] = items
                            
                if ('_la_DEPENDENCIES' in line):
                    spl = line.split('_la_DEPENDENCIES')
                    if len(spl) == 2:
                        projname = spl[0]
                        key = (root, projname)
                        items = spl[1].replace('=', '')
                        items = items.strip()
                        items = items.split()
                        if key in projs:
                            projs[key].extend(items)  
                        else:
                            projs[key] = items  
         
    #print 'makefile_variables.keys()', makefile_variables.keys()
    #print 'makefile_variables', makefile_variables
    
    for line in content.split('\n'):
        if line.startswith('#'): continue
        for k in makefile_variables.keys():
            #print 'key: ', k                                               
            # generate projs !!!
            for v in makefile_variables[k][0]:                
                spl = line.split(v)
                if (len(spl) == 2) and (spl[0] == ''):
                    #print 'variable: ', v
                    #projname = spl[0]
                    id = str(uuid.uuid4()).upper()
                    path_prefix = '.' +  os.path.sep
                    path = root.lstrip(path_prefix)   # remove damned './' from path once forever
                    ############################################################
                    ### assemble key ! ###                    
                    key = (path, k, id)   # (path, projname, id)
                    ############################################################
                    #print 'spl[0]: ', spl[0]
                    #print 'spl[1]: ', spl[1]
                    items = spl[1].replace('=', '')
                    items = items.strip()
                    items = items.split()
                    key_found = False 
                    for k_for in sorted(projs.keys()):
                        if(k_for[0] == path and k_for[1] == k): # path is path, k is projname
                            projs[k_for][0].extend(items)       # items are .c .h files
                            key_found = True
                    if (key_found == False):
                        projs[key] = (items, makefile_variables[k][1])
                    
                                                   
#   for k in sorted(projs.keys()):
#       v = projs[k]


################################################################################
# Get project unique id
def get_project_uuid(project_name):
    for k in projs.keys():
        if(k[1] == project_name):
            return k[2]
    return False

################################################################################
# Returns output project text with added project uuid
# project       - string, text content which is later written to solution content 
# project_name  - string, project name      
def add_project_dependency_uuid(project, project_name):
    project_uuid = get_project_uuid(project_name)
    if(project_uuid != False):
        return project + indent_string + indent_string + '{' + project_uuid + '}' + ' = ' + '{' + project_uuid + '}' + '\n'
    return project
    

################################################################################
# Returns True if versioned module
# project_name  - string, project name
def is_module_versioned(project_name):
    return (   project_name == 'libgwyapp2'     or project_name == 'libgwymodule2' 
            or project_name == 'libgwydgets2'   or project_name == 'libgwydraw2'   
            or project_name == 'libgwyprocess2' or project_name == 'libgwyddion2')
        
                      
################################################################################
# Returns text with added Gwyddion libraries (.lib) dependencies
# project_name - project name string     
def get_project_dependency_gwy_libs(project_name):
    result = ''    
    if(project_name == 'gwyddion'):
        result += 'libgwyapp2-0.lib;'
        result += 'libgwymodule2-0.lib;'
        result += 'libgwydgets2-0.lib;'
        result += 'libgwydraw2-0.lib;'
        result += 'libgwyprocess2-0.lib;'
        result += 'libgwyddion2-0.lib;'
    
    if(project_name == 'libgwyapp2'):
        result += 'libgwymodule2-0.lib;'
        result += 'libgwydgets2-0.lib;'
        result += 'libgwydraw2-0.lib;'
        result += 'libgwyprocess2-0.lib;'
        result += 'libgwyddion2-0.lib;'
        
    if(project_name == 'libgwymodule2'):
        result += 'libgwydgets2-0.lib;'
        result += 'libgwydraw2-0.lib;'
        result += 'libgwyprocess2-0.lib;'
        result += 'libgwyddion2-0.lib;'
        
    if(project_name == 'libgwydgets2'):
        result += 'libgwydraw2-0.lib;'
        result += 'libgwyprocess2-0.lib;'
        result += 'libgwyddion2-0.lib;'
        
    if(project_name == 'libgwydraw2'):
        result += 'libgwyprocess2-0.lib;'
        result += 'libgwyddion2-0.lib;'
        
    if(project_name == 'libgwyprocess2'):
        result += 'libgwyddion2-0.lib;'
        
    if((project_name != 'gwyddion') and (is_module_versioned(project_name) == False)) :
        result += 'libgwyapp2-0.lib;'
        result += 'libgwymodule2-0.lib;'
        result += 'libgwydgets2-0.lib;'
        result += 'libgwydraw2-0.lib;'
        result += 'libgwyprocess2-0.lib;'
        result += 'libgwyddion2-0.lib;'
                
    return result
                                  
################################################################################
# Returns text with added libraries (.lib) dependencies
# project_name - project name string     
def get_project_dependency_libs_win32(project_name):
    result = get_project_dependency_gwy_libs(project_name)    
            
    libs_string_win32 = ';'.join(libs_win32)
        
    result += libs_string_win32
    #result += "cairo.lib;cfitsio.lib;glib-2.0.lib;gtk-win32-2.0.lib;gdk-win32-2.0.lib;gdk_pixbuf-2.0.lib;gio-2.0.lib;gmodule-2.0.lib;gobject-2.0.lib;intl.lib;libgtkglext-win32-1.0.lib;libgdkglext-win32-1.0.lib;libpng.lib;libxml2.lib;OpenGL32.Lib;pango-1.0.lib;pangocairo-1.0.lib;pangoft2-1.0.lib;python27.lib;zlibstat.lib;zlib.lib;%(AdditionalDependencies)"
        
    return result
    
################################################################################
# Returns text with added libraries (.lib) dependencies
# project_name - project name string     
def get_project_dependency_libs_x64(project_name):
    result = get_project_dependency_gwy_libs(project_name)    
            
    libs_string_x64 = ';'.join(libs_x64)
        
    result += libs_string_x64
        
    return result
               
              
################################################################################
# Parse all makefiles in all subfolders of .\gwyddion folder     
# gwyddion_root_folder - 'gwyddion' project root folder           
def parse_makefiles():        
    # replace minidom's function with ours
    xml.dom.minidom.Element.writexml = fixed_writexml
    
    for root, dirs, files in os.walk('.'):
        for name in files:
            if name == 'Makefile.am':
                fullpath = os.path.join(root, name)                                 
                parse_makefile(root, fullpath)
                
    #print '####################################################################                        
    #print projs
    #print '####################################################################        

################################################################################             
def fixed_writexml(self, writer, indent="", addindent="", newl=""):
    # indent = current indentation
    # addindent = indentation to add to higher levels
    # newl = newline string
    writer.write(indent+"<" + self.tagName)

    attrs = self._get_attributes()
    a_names = attrs.keys()
    a_names.sort()

    for a_name in a_names:
        writer.write(" %s=\"" % a_name)
        xml.dom.minidom._write_data(writer, attrs[a_name].value)
        writer.write("\"")
    if self.childNodes:
        if len(self.childNodes) == 1 \
          and self.childNodes[0].nodeType == xml.dom.minidom.Node.TEXT_NODE:
            writer.write(">")
            self.childNodes[0].writexml(writer, "", "", "")
            writer.write("</%s>%s" % (self.tagName, newl))
            return
        writer.write(">%s"%(newl))
        for node in self.childNodes:
            node.writexml(writer,indent+addindent,addindent,newl)
        writer.write("%s</%s>%s" % (indent,self.tagName,newl))
    else:
        writer.write("/>%s"%(newl))

                                   
################################################################################
# Make XML human readable
# element - an Element instance (xml.etree.ElementTree.Element)
def prettify(elem):
    """Return a pretty-printed XML string for the Element.
    """
    rough_string = xml.etree.ElementTree.tostring(elem, 'utf-8')
    reparsed_document = xml.dom.minidom.parseString(rough_string)    
#    prettyxml_string = reparsed_document.toprettyxml(indent="  ")
    prettyxml_string = reparsed_document.toprettyxml(indent="  ", newl="\n", encoding="utf-8")
    return string.replace(prettyxml_string, '&quot;', '"')


################################################################################
# Create all non-existing folders on filename path
# fullpath - filename with path
def create_path(fullpath):        
    dirname, fname = os.path.split(fullpath)
    try:
        os.makedirs(dirname)
    except OSError:
        if os.path.exists(dirname):
            # We are nearly safe
            pass
        else:
            # There was an error on creation, so make sure we know about it
            print "Error: Unable to create folder '" + dirname + "'"
            raise

                        
################################################################################
# Save XML file
# element  - an Element instance (xml.etree.ElementTree.Element)
# filename - XML file name
 
def save_xml(element, filename):     
    create_path(filename)
    
    pretty_string = prettify(element)    
    file(filename, 'w').write(pretty_string)            
    
    
################################################################################
# Get Project Path (.vcxproj)
# path          - file path i.e. './gwyddion/libgwyddion'
# project_name  - project name i.e. 'libgwyddion2'
# returns adapted project path
# output example: ./msvc2015/gwyddion/libgwyddion 
#                 ./msvc2015/gwyddion/app/gwyddion
#                 ./msvc2015/gwyddion/modules/file                   

def get_project_path(path, project_name):
    result = os.path.join('msvc2015', path)    
    if(   project_name == 'libgwymodule2' 
       or project_name == 'libgwydgets2'   or project_name == 'libgwydraw2'   
       or project_name == 'libgwyprocess2' or project_name == 'libgwyddion2'):
       return result 
           
    return os.path.join(result, project_name)
      
        
################################################################################
# Create Project File (.vcxproj)
# path          - file path i.e. './gwyddion/libgwyddion'
# name          - project name i.e. 'libgwyddion2'
# sources       - list of .c files
# headers       - list of .h files
# definitions   - list of .def files (generally only one file) 
# output example: ./msvc2015/gwyddion/libgwyddion/libgwyddion2.vcxproj

def create_vcxproj(path, name, sources, headers, definitions, configuration_type):
    newpath = os.path.join('$(SolutionDir)..\\', path)
    
    post_build_event_files_to_copy_win32_string = ' \n'.join(post_build_event_files_to_copy_win32)
    post_build_event_files_to_copy_x64_string = ' \n'.join(post_build_event_files_to_copy_x64)
    
    # produce non-consistency in source and install paths due yetti
    # 1. 'tools' -> 'tool'
    path_yeti = path.replace('tools', 'tool')
    # 2. strip last subfolder from path
    if(name == 'gwy' or name == 'pygwy'):        
        path_yeti = path_yeti.rstrip(os.path.sep + path_yeti.split(os.path.sep)[-1])    
    post_build_event_modules_run_dir = os.path.join('lib', 'gwyddion', path_yeti) + os.path.sep
    post_build_event_modules_run_dir = post_build_event_modules_run_dir.replace(os.path.sep, '\\')   
       
    # define include directories
    #IncludeDirs = r"$(SolutionDir)gwyddion\..\..\..\gwyddion;$(GTK_DIR)include;$(GTK_DIR)include\atk-1.0;$(GTK_DIR)include\cairo;$(GTK_DIR)include\freetype2;$(GTK_DIR)include\gdk-pixbuf-2.0;$(GTK_DIR)include\glib-2.0;$(GTK_DIR)include\gtk-2.0;$(GTK_DIR)\lib\gtk-2.0\include;$(GTK_DIR)include\gtkglext-1.0;$(GTK_DIR)lib\glib-2.0\include;$(GTK_DIR)include\pango-1.0;$(GTKGLEXT_DIR)gtkglext-1.0\include;$(IncludePath)"
    IncludeDirs_Win32 = ';'.join(include_dirs_win32)
        
    #IncludeDirs_x64 = "$(SolutionDir)gwyddion\\..\\..\\..\\gwyddion;$(GTK_DIR)x64\\include;$(GTK_DIR)x64\\include\\atk-1.0;$(GTK_DIR)x64\\include\\cairo;$(GTK_DIR)x64\\include\\freetype2;$(GTK_DIR)x64\\include\\gdk-pixbuf-2.0;$(GTK_DIR)x64\\include\\glib-2.0;$(GTK_DIR)x64\\include\\gtk-2.0;$(GTK_DIR)x64\\lib\\gtk-2.0\\include;$(GTK_DIR)x64\\include\\gtkglext-1.0;$(GTK_DIR)x64\\lib\\glib-2.0\\include;$(GTK_DIR)x64\\include\\pango-1.0;$(GTKGLEXT_DIR)1.0\\include\\gtkglext-1.0;$(GTKGLEXT_DIR)1.0\\lib\\gtkglext-1.0\\include;$(CFITSIO_DIR)x64;$(LIBXML_DIR)include\\libxml2;$(LIBICONV_DIR)include;$(PYTHON_DIR)include;$(PYTHON_DIR)include\\pygtk-2.0;$(ZLIB_DIR)contrib\\minizip;$(IncludePath)"
    IncludeDirs_x64 = ';'.join(include_dirs_x64)
    
    
    #define library directories
    #LibraryDirs_Win32 = "$(OutDir);$(GTK_DIR)Win32\\lib;c:\\Projects\\gtkglext-1.2.0\\vs2012\\gtkglext\\Release;$(PYTHON_DIR)libs;$(PYTHON_DIR)Lib\\site-packages\\gtk-2.0\\runtime\\lib;$(CFITSIO_DIR)Win32;$(LIBXML_DIR)include\libxml2;$(LIBICONV_DIR)include;$(ZLIB_DIR)x86\lib;$(LibraryPath)"
    LibraryDirs_Win32 = ';'.join(library_dirs_win32)
         
    #LibraryDirs_x64 = "$(OutDir);$(GTK_DIR)x64\\lib;$(GTKGLEXT_DIR)1.0\\lib;$(PYTHON_DIR)libs;$(PYTHON_DIR)Lib\\site-packages\\gtk-2.0\\runtime\\lib;$(CFITSIO_DIR)x64;$(LIBXML_DIR)include\libxml2;$(LIBICONV_DIR)include;$(ZLIB_DIR)x86\lib;$(LibraryPath)"
    LibraryDirs_x64 = ';'.join(library_dirs_x64) 

    # add Project
    Project = Element("Project", DefaultTargets="Build", ToolsVersion="14.0", xmlns="http://schemas.microsoft.com/developer/msbuild/2003")
    
    # add Project Configuration
    ItemGroup = SubElement(Project, "ItemGroup", Label="ProjectConfigurations")
    
    # set project name
    ProjectName = "$(ProjectName)-0"
    if(is_module_versioned(name) == False):
        ProjectName = "$(ProjectName)"        

    ProjectConfiguration = SubElement(ItemGroup, "ProjectConfiguration", Include="Debug|Win32")
    Configuration = SubElement(ProjectConfiguration, "Configuration")
    Configuration.text = "Debug"
    Platform = SubElement(ProjectConfiguration, "Platform")
    Platform.text = "Win32"
    
    ProjectConfiguration = SubElement(ItemGroup, "ProjectConfiguration", Include="Debug|x64")
    Configuration = SubElement(ProjectConfiguration, "Configuration")
    Configuration.text = "Debug"
    Platform = SubElement(ProjectConfiguration, "Platform")
    Platform.text = "x64"
    
    ProjectConfiguration = SubElement(ItemGroup, "ProjectConfiguration", Include="Release|Win32")
    Configuration = SubElement(ProjectConfiguration, "Configuration")
    Configuration.text = "Release"
    Platform = SubElement(ProjectConfiguration, "Platform")
    Platform.text = "Win32"
    
    ProjectConfiguration = SubElement(ItemGroup, "ProjectConfiguration", Include="Release|x64")
    Configuration = SubElement(ProjectConfiguration, "Configuration")
    Configuration.text = "Release"
    Platform = SubElement(ProjectConfiguration, "Platform")
    Platform.text = "x64"
    
    # add Globals
    PropertyGroup = SubElement(Project, "PropertyGroup", Label="Globals")
    RootNamespace = SubElement(PropertyGroup, "RootNamespace")
    RootNamespace.text = name
        
    Import = SubElement(Project, "Import", Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props")
    
    PropertyGroup = SubElement(Project, "PropertyGroup", Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'", Label="Configuration");
    ConfigurationType = SubElement(PropertyGroup, "ConfigurationType");
    ConfigurationType.text = configuration_type     # "DynamicLibrary" or "Application" 
    UseDebugLibraries = SubElement(PropertyGroup, "UseDebugLibraries");
    UseDebugLibraries.text = "true"
    PlatformToolset = SubElement(PropertyGroup, "PlatformToolset");
    PlatformToolset.text = "v140_xp"
    CharacterSet = SubElement(PropertyGroup, "CharacterSet");
    CharacterSet.text = "MultiByte"
    
    PropertyGroup = SubElement(Project, "PropertyGroup", Condition="'$(Configuration)|$(Platform)'=='Debug|x64'", Label="Configuration");
    ConfigurationType = SubElement(PropertyGroup, "ConfigurationType");
    ConfigurationType.text = configuration_type
    UseDebugLibraries = SubElement(PropertyGroup, "UseDebugLibraries");
    UseDebugLibraries.text = "true"
    PlatformToolset = SubElement(PropertyGroup, "PlatformToolset");
    PlatformToolset.text = "v140_xp"
    CharacterSet = SubElement(PropertyGroup, "CharacterSet");
    CharacterSet.text = "MultiByte"
        
    PropertyGroup = SubElement(Project, "PropertyGroup", Condition="'$(Configuration)|$(Platform)'=='Release|Win32'", Label="Configuration");
    ConfigurationType = SubElement(PropertyGroup, "ConfigurationType");
    ConfigurationType.text = configuration_type
    UseDebugLibraries = SubElement(PropertyGroup, "UseDebugLibraries");
    UseDebugLibraries.text = "false"
    PlatformToolset = SubElement(PropertyGroup, "PlatformToolset");
    PlatformToolset.text = "v140_xp"
    CharacterSet = SubElement(PropertyGroup, "CharacterSet");
    CharacterSet.text = "MultiByte"
        
    PropertyGroup = SubElement(Project, "PropertyGroup", Condition="'$(Configuration)|$(Platform)'=='Release|x64'", Label="Configuration");
    ConfigurationType = SubElement(PropertyGroup, "ConfigurationType");
    ConfigurationType.text = configuration_type
    UseDebugLibraries = SubElement(PropertyGroup, "UseDebugLibraries");
    UseDebugLibraries.text = "false"
    PlatformToolset = SubElement(PropertyGroup, "PlatformToolset");
    PlatformToolset.text = "v140_xp"
    CharacterSet = SubElement(PropertyGroup, "CharacterSet");
    CharacterSet.text = "MultiByte"
    
    Import = SubElement(Project, "Import", Project="$(VCTargetsPath)\Microsoft.Cpp.props")
    
    PropertyGroup = SubElement(Project, "PropertyGroup", Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'")
    IncludePath = SubElement(PropertyGroup, "IncludePath")
    IncludePath.text = IncludeDirs_Win32
    LibraryPath = SubElement(PropertyGroup, "LibraryPath")
    LibraryPath.text = LibraryDirs_Win32
    TargetName = SubElement(PropertyGroup, "TargetName")
    TargetName.text = ProjectName     
    
    PropertyGroup = SubElement(Project, "PropertyGroup", Condition="'$(Configuration)|$(Platform)'=='Debug|x64'")
    IncludePath = SubElement(PropertyGroup, "IncludePath")
    IncludePath.text = IncludeDirs_x64
    LibraryPath = SubElement(PropertyGroup, "LibraryPath")
    LibraryPath.text = LibraryDirs_x64
    TargetName = SubElement(PropertyGroup, "TargetName")
    TargetName.text = ProjectName
    
    PropertyGroup = SubElement(Project, "PropertyGroup", Condition="'$(Configuration)|$(Platform)'=='Release|Win32'")
    IncludePath = SubElement(PropertyGroup, "IncludePath")
    IncludePath.text = IncludeDirs_Win32
    LibraryPath = SubElement(PropertyGroup, "LibraryPath")
    LibraryPath.text = LibraryDirs_Win32
    TargetName = SubElement(PropertyGroup, "TargetName")
    TargetName.text = ProjectName
    
    PropertyGroup = SubElement(Project, "PropertyGroup", Condition="'$(Configuration)|$(Platform)'=='Release|x64'")
    IncludePath = SubElement(PropertyGroup, "IncludePath")
    IncludePath.text = IncludeDirs_x64
    LibraryPath = SubElement(PropertyGroup, "LibraryPath")
    LibraryPath.text = LibraryDirs_x64
    TargetName = SubElement(PropertyGroup, "TargetName")
    TargetName.text = ProjectName
    
    ItemDefinitionGroup = SubElement(Project, "ItemDefinitionGroup", Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'")
    ClCompile = SubElement(ItemDefinitionGroup, "ClCompile")
    PreprocessorDefinitions = SubElement(ClCompile, "PreprocessorDefinitions")
    PreprocessorDefinitions.text = "_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)"
    Link = SubElement(ItemDefinitionGroup, "Link")
    AdditionalDependencies = SubElement(Link, "AdditionalDependencies")
    #AdditionalDependencies.text = "libgwyapp2-0.lib;libgwyddion2-0.lib;libgwydgets2-0.lib;libgwydraw2-0.lib;libgwymodule2-0.lib;libgwyprocess2-0.lib;cairo.lib;glib-2.0.lib;gtk-win32-2.0.lib;gdk-win32-2.0.lib;gdk_pixbuf-2.0.lib;gio-2.0.lib;gmodule-2.0.lib;gobject-2.0.lib;intl.lib;libgtkglext-win32-1.0.lib;libgdkglext-win32-1.0.lib;libpng.lib;libxml2.lib;OpenGL32.Lib;pango-1.0.lib;pangocairo-1.0.lib;pangoft2-1.0.lib;python27.lib;Qt5Gui.lib;zlib.lib;%(AdditionalDependencies)"
    AdditionalDependencies.text = get_project_dependency_libs_win32(name)
    #ModuleDefinitionFile = SubElement(Link, "ModuleDefinitionFile")
    #ModuleDefinitionFile.text = name + ".def"
    if len(definitions) > 0:
        ModuleDefinitionFile = SubElement(Link, "ModuleDefinitionFile")
        ModuleDefinitionFile.text =  os.path.join(newpath, definitions[0])
        #ModuleDefinitionFile.text = definitions[0]
    SubSystem = SubElement(Link, "SubSystem")
    SubSystem.text = "Windows"
    if(name == 'gwyddion'):
        AdditionalOptions = SubElement(Link, "AdditionalOptions")
        AdditionalOptions.text = "/ENTRY:mainCRTStartup %(AdditionalOptions)"
        PostBuildEvent = SubElement(ItemDefinitionGroup, "PostBuildEvent")
        Command = SubElement(PostBuildEvent, "Command")
        Command.text = post_build_event_files_to_copy_win32_string
    #module_path = os.path.join('gwyddion', 'modules')
    #module_path = os.path.join('.', 'modules')
    if(path.startswith('modules') and (name not in excluded_projects_win32)):
        PostBuildEvent = SubElement(ItemDefinitionGroup, "PostBuildEvent")
        Command = SubElement(PostBuildEvent, "Command")
        # example: Command.text = 'xcopy /y /d "$(OutDir)$(ProjectName).dll" "$(OutDir)'lib\\gwyddion\\modules\\file\\"' 
        Command.text = modules_copy_command_string.format(post_build_event_modules_run_dir)
        
    ItemDefinitionGroup = SubElement(Project, "ItemDefinitionGroup", Condition="'$(Configuration)|$(Platform)'=='Debug|x64'")
    ClCompile = SubElement(ItemDefinitionGroup, "ClCompile")
    PreprocessorDefinitions = SubElement(ClCompile, "PreprocessorDefinitions")
    PreprocessorDefinitions.text = "_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)"
    Link = SubElement(ItemDefinitionGroup, "Link")
    AdditionalDependencies = SubElement(Link, "AdditionalDependencies")
    AdditionalDependencies.text = get_project_dependency_libs_x64(name)
    if len(definitions) > 0:
        ModuleDefinitionFile = SubElement(Link, "ModuleDefinitionFile")
        ModuleDefinitionFile.text =  os.path.join(newpath, definitions[0])
    SubSystem = SubElement(Link, "SubSystem")
    SubSystem.text = "Windows"
    if(name == 'gwyddion'):
        AdditionalOptions = SubElement(Link, "AdditionalOptions")
        AdditionalOptions.text = "/ENTRY:mainCRTStartup %(AdditionalOptions)"
        PostBuildEvent = SubElement(ItemDefinitionGroup, "PostBuildEvent")
        Command = SubElement(PostBuildEvent, "Command")
        Command.text = post_build_event_files_to_copy_x64_string
    module_path = os.path.join('gwyddion', 'modules')
    if(path.startswith(module_path) and name not in excluded_projects_x64):
        PostBuildEvent = SubElement(ItemDefinitionGroup, "PostBuildEvent")
        Command = SubElement(PostBuildEvent, "Command")
        Command.text = modules_copy_command_string.format(post_build_event_modules_run_dir)
    
    ItemDefinitionGroup = SubElement(Project, "ItemDefinitionGroup", Condition="'$(Configuration)|$(Platform)'=='Release|Win32'")
    ClCompile = SubElement(ItemDefinitionGroup, "ClCompile")
    PreprocessorDefinitions = SubElement(ClCompile, "PreprocessorDefinitions")
    PreprocessorDefinitions.text = "_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)"
    Link = SubElement(ItemDefinitionGroup, "Link")
    AdditionalDependencies = SubElement(Link, "AdditionalDependencies")
    AdditionalDependencies.text = get_project_dependency_libs_win32(name)
    if len(definitions) > 0:
        ModuleDefinitionFile = SubElement(Link, "ModuleDefinitionFile")
        ModuleDefinitionFile.text =  os.path.join(newpath, definitions[0])
    SubSystem = SubElement(Link, "SubSystem")
    SubSystem.text = "Windows"
    if(name == 'gwyddion'):
        AdditionalOptions = SubElement(Link, "AdditionalOptions")
        AdditionalOptions.text = "/ENTRY:mainCRTStartup %(AdditionalOptions)"
        PostBuildEvent = SubElement(ItemDefinitionGroup, "PostBuildEvent")
        Command = SubElement(PostBuildEvent, "Command")
        Command.text = post_build_event_files_to_copy_win32_string
    module_path = os.path.join('gwyddion', 'modules')
    if(path.startswith(module_path) and (name not in excluded_projects_win32)):
        PostBuildEvent = SubElement(ItemDefinitionGroup, "PostBuildEvent")
        Command = SubElement(PostBuildEvent, "Command")
        Command.text = modules_copy_command_string.format(post_build_event_modules_run_dir)
    
    ItemDefinitionGroup = SubElement(Project, "ItemDefinitionGroup", Condition="'$(Configuration)|$(Platform)'=='Release|x64'")
    ClCompile = SubElement(ItemDefinitionGroup, "ClCompile")
    PreprocessorDefinitions = SubElement(ClCompile, "PreprocessorDefinitions")
    PreprocessorDefinitions.text = "_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)"
    Link = SubElement(ItemDefinitionGroup, "Link")
    AdditionalDependencies = SubElement(Link, "AdditionalDependencies")
    AdditionalDependencies.text = get_project_dependency_libs_x64(name)
    if len(definitions) > 0:
        ModuleDefinitionFile = SubElement(Link, "ModuleDefinitionFile")
        ModuleDefinitionFile.text =  os.path.join(newpath, definitions[0])
    SubSystem = SubElement(Link, "SubSystem")
    SubSystem.text = "Windows"
    if(name == 'gwyddion'):
        AdditionalOptions = SubElement(Link, "AdditionalOptions")
        AdditionalOptions.text = "/ENTRY:mainCRTStartup %(AdditionalOptions)"
        PostBuildEvent = SubElement(ItemDefinitionGroup, "PostBuildEvent")
        Command = SubElement(PostBuildEvent, "Command")
        Command.text = post_build_event_files_to_copy_x64_string
    module_path = os.path.join('gwyddion', 'modules')
    if(path.startswith(module_path) and name not in excluded_projects_x64):
        PostBuildEvent = SubElement(ItemDefinitionGroup, "PostBuildEvent")
        Command = SubElement(PostBuildEvent, "Command")
        Command.text = modules_copy_command_string.format(post_build_event_modules_run_dir)
        
    # add .c and .h and .def files
    #newpath = '$(SolutionDir)..\\..\\' + path
    #newpath = os.path.join('$(SolutionDir)..\\..\\', path)
    
    ItemGroup = SubElement(Project, "ItemGroup");
    for v in headers:
        header_filename = os.path.join(newpath, v) 
        ClInclude = SubElement(ItemGroup, "ClInclude", Include=header_filename)
        #ClInclude = SubElement(ItemGroup, "ClInclude", Include=(newpath + '/' + v))
    ItemGroup = SubElement(Project, "ItemGroup")
    for v in sources:
        source_filename = os.path.join(newpath, v)
        ClCompile = SubElement(ItemGroup, "ClCompile", Include=source_filename)
        #ClCompile = SubElement(ItemGroup, "ClCompile", Include=(newpath + '/' + v))
        if (name in excluded_projects_win32):            
            ExcludedFromBuild = SubElement(ClCompile, "ExcludedFromBuild", Condition = "'$(Configuration)|$(Platform)'=='Debug|Win32'");
            ExcludedFromBuild.text = "true"
            ExcludedFromBuild = SubElement(ClCompile, "ExcludedFromBuild", Condition = "'$(Configuration)|$(Platform)'=='Release|Win32'");
            ExcludedFromBuild.text = "true"            
        if (name in excluded_projects_x64):
            ExcludedFromBuild = SubElement(ClCompile, "ExcludedFromBuild", Condition = "'$(Configuration)|$(Platform)'=='Debug|x64'");
            ExcludedFromBuild.text = "true"
            ExcludedFromBuild = SubElement(ClCompile, "ExcludedFromBuild", Condition = "'$(Configuration)|$(Platform)'=='Release|x64'");
            ExcludedFromBuild.text = "true"
    ItemGroup = SubElement(Project, "ItemGroup")
    for v in definitions:
        definition_filename = os.path.join(newpath, v)
        ClInclude = SubElement(ItemGroup, "None", Include=definition_filename)
        #ClInclude = SubElement(ItemGroup, "None", Include=(newpath + '/' + v))
        
    Import = SubElement(Project, "Import", Project="$(VCTargetsPath)\Microsoft.Cpp.targets")
    ImportGroup = SubElement(Project, "ImportGroup", Label="ExtensionTargets")
    
    #filename = os.getcwd() + '//msvc2015//' + path + '//' + name + '.vcxproj'    
    #filename = 'msvc2015/' + path + '/' + name + '.vcxproj'
    #filename = os.path.join('msvc2015', path, name + '.vcxproj')
    filename = get_project_path(path, name)
    filename = os.path.join(filename, name + '.vcxproj')
    filename = os.path.abspath(filename)
    save_xml(Project, filename)
        
    # TODO: uncomment next line!!!
    print filename
    
#    for name, files in proj_files.iteritems():
#        print name, files


################################################################################
# Create Project Filters File (.vcxproj.filters)
# path          - file path i.e. './gwyddion/libgwyddion'
# name          - project name i.e. 'libgwyddion2'
# sources       - list of .c files
# headers       - list of .h files
# definitions   - list of .def files (generally only one file) 
# output example: ./msvc2015/gwyddion/libgwyddion/libgwyddion2.vcxproj.filters 

def create_vcxproj_filters(path, name, sources, headers, definitions):
    # add Project
    Project = Element("Project", DefaultTargets="Build", ToolsVersion="4.0", xmlns="http://schemas.microsoft.com/developer/msbuild/2003")
    
    # add Filters
    ItemGroup = SubElement(Project, "ItemGroup", Label="ProjectConfigurations")

    Filter = SubElement(ItemGroup, "Filter", Include="Source Files")
    UniqueIdentifier = SubElement(Filter, "UniqueIdentifier")
    UniqueIdentifier.text = "{4FC737F1-C7A5-4376-A066-2A32D752A2FF}"
    Extensions = SubElement(Filter, "Extensions")
    Extensions.text = "cpp;c;cc;cxx;def;odl;idl;hpj;bat;asm;asmx"
    
    Filter = SubElement(ItemGroup, "Filter", Include="Header Files")
    UniqueIdentifier = SubElement(Filter, "UniqueIdentifier")
    UniqueIdentifier.text = "{93995380-89BD-4b04-88EB-625FBE52EBFB}"
    Extensions = SubElement(Filter, "Extensions")
    Extensions.text = "h;hh;hpp;hxx;hm;inl;inc;xsd"
    
    # add .c and .h and .def files
    newpath = path.replace('./', '$(SolutionDir)..\\', 1)
    newpath = newpath.replace('.\\', '$(SolutionDir)..\\', 1)
    ItemGroup = SubElement(Project, "ItemGroup");
    for v in headers:
        ClInclude = SubElement(ItemGroup, "ClInclude", Include=(newpath + '/' + v));
        Filter = SubElement(ClInclude, "Filter");
        Filter.text = "Header Files"
    for v in sources:
        ClCompile = SubElement(ItemGroup, "ClCompile", Include=(newpath + '/' + v));
        Filter = SubElement(ClCompile, "Filter");
        Filter.text = "Source Files"
    ItemGroup = SubElement(Project, "ItemGroup");
    for v in definitions:
        ClCompile = SubElement(ItemGroup, "None", Include=(newpath + '/' + v));
        Filter = SubElement(ClCompile, "Filter");
        Filter.text = "Source Files"
    ItemGroup = SubElement(Project, "ItemGroup");
        
    #filename = os.getcwd() + '//msvc2015//' + path + '//' + name + '.vcxproj.filters'
    #filename = 'msvc2015/' + path + '/' + name + '.vcxproj.filters'
    #filename = os.path.join('msvc2015', path, name + '.vcxproj.filters')
    filename = get_project_path(path, name)
    filename = os.path.join(filename, name + '.vcxproj.filters')
    filename = os.path.abspath(filename)
    save_xml(Project, filename)
    #TODO: uncomment next line
    print filename
    

################################################################################
# Generate Definition File (.def)
# path          - file path, i.e. './gwyddion/libgwyddion'
# name          - project name, i.e. 'libgwyddion2'
# definitions   - list of .def files (generally only one file), i.e. 'libgwyddion2.def'
# note: this function must be run on Linux machine after compiling Gwyddion and generating .c .h files
#       which are needed for 'make' commands        
#       first run explicitly './autogen.sh' and 'make'
# example:
# command: make -C /home/temp/gwyddion/libgwyddion libgwyddion2.def  
# output:  /home/temp/gwyddion/libgwyddion/libgwyddion2.def 

def create_def_file(path, name, definitions):
    for v in definitions:
        path = os.path.abspath(path)  # normalize path, i.e. 'dir1/./dir2'
        cmd = ('make', '-C', path, v)
        print "path=", path
        print "v=", v
        subprocess.call(cmd)
            
    
################################################################################
# Create:   Project files (.vcxproj)
#           Filters files (.vcxproj_filters)

def create_project_and_filters_files():
    proj_count = 0 
    for k in sorted(projs.keys()):
        v = projs[k][0]
        sources = [x for x in v if x.endswith('.c')]
        sources.sort()
        headers = [x for x in v if x.endswith('.h')]
        headers.sort()
        definitions = [x for x in v if x.endswith('.def')]
        definitions.sort()
        #print k
        #print 'SOURCES:', sources
        #print 'HEADERS:', headers
        #print 'DEFS:', definitions
        
        create_vcxproj(k[0], k[1], sources, headers, definitions, projs[k][1])
        create_vcxproj_filters(k[0], k[1], sources, headers, definitions)
        
        proj_count = proj_count + 1 
        
    if(len(excluded_projects_win32) > 0):
        print "Excluded projects from build Win32:"
        print excluded_projects_win32
    if(len(excluded_projects_x64) > 0):
        print "Excluded projects from build x64:"
        print excluded_projects_x64
    print "Total number of created projects:", proj_count
                                 
            
################################################################################
# Create Definition Files (.def)

def create_def_files(): 
    n = 0
    if platform.system() == 'Linux':
        for k in sorted(projs.keys()):
            v = projs[k][0]
            sources = [x for x in v if x.endswith('.c')]
            sources.sort()
            headers = [x for x in v if x.endswith('.h')]
            headers.sort()
            definitions = [x for x in v if x.endswith('.def')]
            definitions.sort()
                    
            n = n + len(definitions) 
            create_def_file(k[0], k[1], definitions)  
            
        print "Total number of created files:", n            
    else:
        print "Error: Creating definition files failed"
        print "Note: Definition files can be generated on Linux machine only!"
        
    
################################################################################
# Copy Definition Files (.def)
        
def copy_def_files():  
    n = 0      
    for k in sorted(projs.keys()):
        v = projs[k][0]
        definitions = [x for x in v if x.endswith('.def')]
        definitions.sort()
                
        path = k[0]
        name = k[1] 
        definition_name = ''
        if(len(definitions) > 0):
            definition_name = definitions[0]        
        if(len(definition_name) == 0) : continue             
        
        src_filename = os.path.join(path, definition_name)
        src_filename = os.path.abspath(src_filename)
        
        if(os.path.exists(src_filename) == True):
            #dst_filename1 = os.path.join('msvc2015', path)
            #dst_filename1 = os.path.join(dst_filename1, definition_name)
            #dst_filename1 = os.path.abspath(dst_filename1)        
        
            dst_filename2 = os.path.join('msvc2015', 'generated-files-def', path)
            dst_filename2 = os.path.join(dst_filename2, definition_name)                
            dst_filename2 = os.path.abspath(dst_filename2)
                
            n = n + 1
            create_path(dst_filename2)
            print "from:", src_filename
            #print "to:  ", dst_filename1
            print "to:  ", dst_filename2        
            shutil.copy(src_filename, dst_filename2)  
                
    print "Total number of copied files:", n        
        
################################################################################
# Copy Files Generated by './autogen.sh' and 'make' command on Linux machine
# Check all subfolders of gwyddion_root_folder folder; i.e. './gwyddion'
        
def copy_gen_files():
    n = 0
    solution_dir = os.path.join('.', 'msvc2015') 
    #for root, dirs, files in os.walk('gwyddion'):
    for root, dirs, files in os.walk('.'):
        for name in files:
            if(root.startswith(solution_dir) == False):
              full_path = os.path.join(root, name)
              full_path = os.path.normpath(full_path)            
              for v in generated_files_by_autotools: 
                  gen_file = os.path.normpath(v)
                  #print "full_path", full_path
                  #print "gen_file", gen_file
                  if(full_path.endswith(gen_file) == True):                    
                      src_filename = full_path
                      src_filename = os.path.abspath(src_filename)
                      
                      dst_filename = os.path.join('msvc2015','generated-files-ch', gen_file)
                      dst_filename = os.path.abspath(dst_filename)
                                                               
                      n = n + 1 
                      create_path(dst_filename)
                      shutil.copy(src_filename, dst_filename)
                      
                      print "from:", src_filename
                      print "to  :", dst_filename                                                                                      

    print "Total number of copied files:", n        
        
################################################################################
# Create Solution file (.sln)
# gwyddion_root_folder - 'gwyddion' project root folder

def create_sln():
    #indent_string = '    '    
    nested_projects_block = ''        
    
    #filename = os.path.join('msvc2015', 'gwyddion', 'gwyddion.sln')
    filename = os.path.join('msvc2015', 'gwyddion.sln')
    filename = os.path.abspath(filename)
    create_path(filename)
    fh = file(filename, 'w')            
    fh = file(filename, 'a')
    fh.write('Microsoft Visual Studio Solution File, Format Version 12.00\n')
    fh.write('# Visual Studio 14\n')
    fh.write('VisualStudioVersion = 14.0.22823.1\n')
    fh.write('MinimumVisualStudioVersion = 10.0.40219.1\n')
    
    #####
    if(0):
        keys = projs.keys()
        keys = [x[0] for x in keys]
        keys.sort()
        print keys
        for k in keys:
            print k
        raise SystemExit
    #####
        
    # write project UUIDs
    id_main = '8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942'
    id_main_sf = '2150E333-8FDC-42A3-9474-1A3956D46DE8'
    for key in sorted(projs):
        #print key, 'corresponds to project name', key[1]
        path = key[0]
        name = key[1]
        #filename_rel = path + '/' + name + '.vcxproj'            
        #filename_rel = os.path.join(path, name + '.vcxproj')
        filename_rel = get_project_path(path, name)
        filename_rel = os.path.join(filename_rel, name + '.vcxproj')
        filename_rel = os.path.normpath(filename_rel)  
        filename_rel = filename_rel.lstrip('msvc2015')
        filename_rel = filename_rel.lstrip(os.path.sep) 
        #filename_rel = filename_rel.lstrip('gwyddion')
        #filename_rel = filename_rel.lstrip(gwyddion_root_folder)
        folder_list = []
        filename_rel = filename_rel.lstrip(os.path.sep)
        folder_list = filename_rel.split(os.path.sep)                
            
        del folder_list[-1]
        
        #print '######'
        #print 'path: ', path
        #print 'name: ', name                                                                                                                                 
                        
        path_simple = filename_rel
                        
        #print 'folder_list: ', folder_list
                        
        id_sp = 0
        solution_folders_local = {}
        n = len(path_simple.split(os.path.sep)) 
        for i in reversed(range(n)):
            sub_path_len = 0
            path_simple = filename_rel.rsplit(os.path.sep, n-i-1)[0]
            parent_path = filename_rel.rsplit(os.path.sep, n-i)[0]
            last_element = parent_path.split(os.path.sep)[-1]
            
            sub_path_tmp = path_simple.rsplit(os.path.sep, i-1)[0]                                       
                                                    
            id_path_parent = 0
            if (parent_path not in solution_folders):
                id_path_parent = str(uuid.uuid4()).upper()
                if (os.path.sep not in parent_path):
                    # add project folder                            
                    project = 'Project("{%s}") = "%s", "%s", "{%s}"\n' %(id_main_sf, parent_path, parent_path, id_path_parent)
                    project = project + indent_string                                                                                                                
                    fh.write(project)                                                                                    
                    fh.write('EndProject\n')
                    solution_folders[parent_path] = id_path_parent                                                    
                    #print 'parent_path ADDED to solution_folders: ', parent_path
                else:
                    # get last folder (leaf)
                    last_folder = parent_path.rsplit(os.path.sep, 1)[1]
                    project = 'Project("{%s}") = "%s", "%s", "{%s}"\n' %(id_main_sf, last_folder, last_folder, id_path_parent)
                    fh.write(project)
                    fh.write('EndProject\n')
                    solution_folders[parent_path] = id_path_parent
                    solution_folders[last_folder] = id_path_parent                                                       
                    #print 'parent_path ADDED to solution_folders: ', parent_path
                    #print 'last_folder ADDED to solution_folders: ', last_folder
            else:
                id_path_parent = solution_folders[parent_path] 
                
            id_path_simple = 0
            if (path_simple not in solution_folders):
                #id_path_simple = str(uuid.uuid4()).upper()
                id_path_simple = get_project_uuid(name)
                if(id_path_simple != 0):
                    if(path_simple.endswith('.vcxproj')):
                        # add project
                        project = 'Project("{%s}") = "%s", "%s", "{%s}"\n' %(id_main, name, path_simple, id_path_simple)                                   
                        # add project dependencies
                        project = project + indent_string
                        project = project + 'ProjectSection(ProjectDependencies) = postProject\n'                                                                                        
                        
                        if(name == 'gwyddion'):
                            project = add_project_dependency_uuid(project, 'libgwyapp2')
                            project = add_project_dependency_uuid(project, 'libgwymodule2')
                            project = add_project_dependency_uuid(project, 'libgwydgets2')
                            project = add_project_dependency_uuid(project, 'libgwydraw2')
                            project = add_project_dependency_uuid(project, 'libgwyprocess2')
                            project = add_project_dependency_uuid(project, 'libgwyddion2')
                        
                        if(name == 'libgwyapp2'):
                            project = add_project_dependency_uuid(project, 'libgwymodule2')
                            project = add_project_dependency_uuid(project, 'libgwydgets2')
                            project = add_project_dependency_uuid(project, 'libgwydraw2')
                            project = add_project_dependency_uuid(project, 'libgwyprocess2')
                            project = add_project_dependency_uuid(project, 'libgwyddion2')
                            
                        if(name == 'libgwymodule2'):
                            project = add_project_dependency_uuid(project, 'libgwydgets2')
                            project = add_project_dependency_uuid(project, 'libgwydraw2')
                            project = add_project_dependency_uuid(project, 'libgwyprocess2')
                            project = add_project_dependency_uuid(project, 'libgwyddion2')
                            
                        if(name == 'libgwydgets2'):
                            project = add_project_dependency_uuid(project, 'libgwydraw2')
                            project = add_project_dependency_uuid(project, 'libgwyprocess2')
                            project = add_project_dependency_uuid(project, 'libgwyddion2')
                            
                        if(name == 'libgwydraw2'):
                            project = add_project_dependency_uuid(project, 'libgwyprocess2')
                            project = add_project_dependency_uuid(project, 'libgwyddion2')
                            
                        if(name == 'libgwyprocess2'):
                            project = add_project_dependency_uuid(project, 'libgwyddion2')
                            
                        if(name != 'gwyddion' and name != 'libgwyapp2' and name != 'libgwymodule2'
                           and name != 'libgwydgets2' and name != 'libgwydraw2' and name != 'libgwyprocess2'
                           and name != 'libgwyddion2'):
                            project = add_project_dependency_uuid(project, 'libgwyapp2')
                            project = add_project_dependency_uuid(project, 'libgwymodule2')
                            project = add_project_dependency_uuid(project, 'libgwydgets2')
                            project = add_project_dependency_uuid(project, 'libgwydraw2')
                            project = add_project_dependency_uuid(project, 'libgwyprocess2')
                            project = add_project_dependency_uuid(project, 'libgwyddion2')
                                                                                                                                                                                                                
                        project = project + indent_string + 'EndProjectSection\n'                                                                                            
                    else:
                        # add folder 
                        project = 'Project("{%s}") = "%s", "%s", "{%s}"\n' %(id_main_sf, path_simple, path_simple, id_path_simple)
                    fh.write(project)
                    fh.write('EndProject\n')
                    solution_folders[path_simple] = id_path_simple                        
                    #print 'path_simple ADDED to solution_folders: ', path_simple
                    
            else:
                id_path_simple = solution_folders[path_simple] 
                
            # write nested project hierarchy
            newline = indent_string + indent_string + '{%s} = {%s}\n' %(id_path_simple, id_path_parent)
            nested_projects_block = nested_projects_block + newline                                                                                                                                                           
                                                                                                                                        
                
            #project = 'Project("{%s}") = "%s", "%s", "{%s}"\n' %(id_main_sf, path, path, id)
            #fh.write(project)
            #fh.write('EndProject\n')
                        
                
            # add projects
            #project = 'Project("{%s}") = "%s", "%s", "{%s}"\n' %(id_main, name, filename_rel, id)
            #fh.write(project)
            #fh.write('EndProject\n')
            
        #####################

        
    fh.write('Global\n')
    fh.write(indent_string)
    fh.write('GlobalSection(SolutionConfigurationPlatforms) = preSolution\n')
    fh.write(indent_string)
    fh.write(indent_string)
    fh.write('Debug|x64 = Debug|x64\n')
    fh.write(indent_string)
    fh.write(indent_string)
    fh.write('Debug|x86 = Debug|x86\n')
    fh.write(indent_string)
    fh.write(indent_string)
    fh.write('Release|x64 = Release|x64\n')
    fh.write(indent_string)
    fh.write(indent_string)
    fh.write('Release|x86 = Release|x86\n')
    fh.write(indent_string)
    fh.write('EndGlobalSection\n')
    fh.write(indent_string)
    fh.write('GlobalSection(ProjectConfigurationPlatforms) = postSolution\n')
    
    # write configurations; projects which arte excluded from build are not written
    # this exclusion is correct but it is not obvious for user to see excluded projects immediatelly
    # so exclusion is implemented in create_vcxproj function by excluding all .c files in project
    # see "ClCompile", "ExcludedFromBuild" .vcxproj section
    # uncomment this brach if needed     
    if(1):
        for key in sorted(projs):
            #print key, 'corresponds to project name', key[1]
            name = key[1]
            id_path_simple = key[2]

            fh.write(indent_string)
            fh.write(indent_string)
            fh.write('{%s}.Debug|x86.ActiveCfg = Debug|Win32\n' %(id_path_simple))
            fh.write(indent_string)
            fh.write(indent_string)
            fh.write('{%s}.Release|x86.ActiveCfg = Release|Win32\n' %(id_path_simple))
            fh.write(indent_string)
            fh.write(indent_string)
            fh.write('{%s}.Debug|x64.ActiveCfg = Debug|x64\n' %(id_path_simple))            
            fh.write(indent_string)
            fh.write(indent_string)
            fh.write('{%s}.Release|x64.ActiveCfg = Release|x64\n' %(id_path_simple))
                                                    
            if((id_path_simple != 0) and (name not in excluded_projects_win32)):                
                fh.write(indent_string)
                fh.write(indent_string)
                fh.write('{%s}.Debug|x86.Build.0 = Debug|Win32\n' %(id_path_simple))
                fh.write(indent_string)
                fh.write(indent_string)                
                fh.write('{%s}.Release|x86.Build.0 = Release|Win32\n' %(id_path_simple))
                
            if((id_path_simple != 0) and (name not in excluded_projects_x64)):    
                fh.write(indent_string)
                fh.write(indent_string)
                fh.write('{%s}.Debug|x64.Build.0 = Debug|x64\n' %(id_path_simple))
                fh.write(indent_string)
                fh.write(indent_string)
                fh.write('{%s}.Release|x64.Build.0 = Release|x64\n' %(id_path_simple))

    
    fh.write(indent_string)
    fh.write('EndGlobalSection\n')
    
    fh.write(indent_string)    
    fh.write('GlobalSection(SolutionProperties) = preSolution\n')
    fh.write(indent_string)
    fh.write(indent_string)
    fh.write('HideSolutionNode = FALSE\n')
    fh.write(indent_string)
    fh.write('EndGlobalSection\n')
    
    # write nested projects block
    fh.write(indent_string)
    fh.write('GlobalSection(NestedProjects) = preSolution\n')
    fh.write(nested_projects_block)
    fh.write(indent_string)
    fh.write('EndGlobalSection\n')
    
    fh.write('EndGlobal\n')
    
    fh.close()
    
    # TODO: uncomment next line!!!
    print filename


################################################################################
################################################################################

arg_parser = argparse.ArgumentParser(description="Generate Gwyddion Visual Studio Solution.", formatter_class=argparse.RawDescriptionHelpFormatter,
epilog = "This script implements 5 steps:\n\
Step 1: Create project (.vcxproj) and filters (.vcxproj.filters) files.\n\
Step 2: Create solution (.sln).\n\
Step 3: Create definition (.def) files.\n\
        Linux machine only.\n\
Step 4: Copy definition (.def) files to 'generated-files-def' folder.\n\
        Linux machine only.\n\
Step 5: Copy generated (.c, .h) files to 'generated-files-ch' folder.\n\
        Linux machine only.\n\
Steps 3, 4, 5: Compile Gwyddion on Linux machine first to generate .c .h files (run './autogen.sh' and 'make').\n")
#arg_parser.add_argument('gwyddion_root_folder', metavar='folder', help="name of 'gwyddion' source code root folder containing 'Makefile.am' files. 'gwyddion' folder must be in the same folder as this script.",)
#args = arg_parser.parse_args()

#set_gwyddion_root_folder_to_global_lists(args.gwyddion_root_folder)
                       
parse_makefiles()           

print "Step 1 of 5"
print "Creating project (.vcxproj) and filters (.vcxproj.filters) files:"
create_project_and_filters_files()

print "\nStep 2 of 5"
print "Creating solution (.sln):"
create_sln()

print "\nStep 3 of 5"
print "Creating definition (.def) files:"
create_def_files()

print "\nStep 4 of 5"
print "Copying definition (.def) files:"
copy_def_files()

print "\nStep 5 of 5"
print "Copying generated (.c, .h) files:"
print "Note: compile 'gwyddion' to create .c, .h files (run './autogen.sh' and 'make')"
copy_gen_files()


#config_h = '''
#'''
#gwyconfig_h = '''
#'''
#file('config.h', 'w').write(config_h)
#file('gwyconfig.h', 'w').write(gwyconfig_h)
