# Copyright (c) 2003 IBM, Inc. and others
#
#  genldml_resources.mak
#
#      Windows nmake makefile for compiling and packaging the resources
#      for for the ICU sample program "genldml".
#
#      This makefile is normally invoked by the pre-link step in the
#      MSVC project file for genldml

#
#  List of resource files to be built.
#    When adding a resource source (.txt) file for a new locale, the corresponding
#    .res file must be added to this list, AND to the file res-file-list.txt
#
RESFILES= root.res 

#
#  ICUDIR   the location of ICU, used to locate the tools for
#           compiling and packaging resources.
#
ICUDIR=$(ICU_ROOT)

#
#  File name extensions for inference rule matching.
#    clear out the built-in ones (for .c and the like), and add
#    the definition for .txt to .res.
#
.SUFFIXES :
.SUFFIXES : .txt

#
#  Inference rule, for compiling a .txt file into a .res file.
#  -t fools make into thinking there are files such as es.res, etc
#
.txt.res:
        $(ICUDIR)\bin\genrb -t --package-name genldml_resources -d . $*.txt


#
#  all - nmake starts here by default
#
all: genldml_resources.dll

genldml_resources.dll: $(RESFILES)
        $(ICUDIR)\bin\pkgdata --name genldml_resources -v -O R:$(ICUDIR) --mode dll -d . res-files-list.txt
