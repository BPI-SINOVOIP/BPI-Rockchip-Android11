#!/bin/sh

output=$1
filter=$2
testlist=$3

echo "<?xml version=\"1.0\"?>" > $output
echo "<!DOCTYPE refsect1 PUBLIC \"-//OASIS//DTD DocBook XML V4.3//EN\"" >> $output
echo "               \"http://www.oasis-open.org/docbook/xml/4.3/docbookx.dtd\"" >> $output
echo "[" >> $output
echo "  <!ENTITY % local.common.attrib \"xmlns:xi  CDATA  #FIXED 'http://www.w3.org/2003/XInclude'\">" >> $output
echo "  <!ENTITY version SYSTEM \"version.xml\">" >> $output
echo "]>" >> $output
echo "<refsect1>" >> $output
echo "<title>Programs</title>" >> $output
echo "<informaltable pgwide=\"1\" frame=\"none\"><tgroup cols=\"2\"><tbody>" >> $output
for test in `cat $testlist | tr ' ' '\n' | grep "^$filter" | sort`; do
	echo "<row><entry role=\"program_name\">" >> $output;
	echo "<link linkend=\"$test\">$test</link></entry></row>" >> $output;
done;
echo "</tbody></tgroup></informaltable>" >> $output
echo "</refsect1>" >> $output
