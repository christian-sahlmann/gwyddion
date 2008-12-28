<?xml version="1.0"?>
<!-- Generate GConf thumbnailer schemas from mime-types handled by gwyddion
     (gwyddion.xml).  We can, of course, preview anything we can load.  Some
     files might contain hardly previewable data (spectra, ...) so the
     thumbnailer may sometimes fail, but we can't tell when until we load
     the files.  -->
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:m='http://www.freedesktop.org/standards/shared-mime-info'>
<xsl:output method="xml"
            encoding="utf-8"
            indent="yes"/>

<xsl:template match="m:mime-info">
  <!-- Keep GENERATED quoted to prevent match here. -->
  <xsl:comment> This is a <xsl:text>GENERATED</xsl:text> file. </xsl:comment>
  <gconfschemafile>
    <schemalist>
    <xsl:for-each select="m:mime-type">
      <schema>
        <key>/schemas/desktop/gnome/thumbnailers/<xsl:value-of select="translate(@type,'/','@')"/>/enable</key>
        <applyto>/desktop/gnome/thumbnailers/<xsl:value-of select="translate(@type,'/','@')"/>/enable</applyto>
        <owner>gwyddion</owner>
        <type>bool</type>
        <default>true</default>
        <locale name="C">
          <short>Enable thumbnailing of <xsl:value-of select="m:comment"/></short>
          <long>True enables thumbnailing and false disables the creation of new thumbnails</long>
        </locale>
      </schema>
      <xsl:text>
</xsl:text>
      <schema>
        <key>/schemas/desktop/gnome/thumbnailers/<xsl:value-of select="translate(@type,'/','@')"/>/command</key>
        <applyto>/desktop/gnome/thumbnailers/<xsl:value-of select="translate(@type,'/','@')"/>/command</applyto>
        <owner>gwyddion</owner>
        <type>string</type>
        <!-- Use absolute path to make installations in an obscure location
             Just Work(TM). -->
        <default>@bindir@/gwyddion-thumbnailer gnome2 %s %i %o</default>
        <locale name="C">
          <short>Thumbnail command for <xsl:value-of select="m:comment"/></short>
          <long>Valid command plus arguments for the <xsl:value-of select="m:comment"/> thumbnailer</long>
        </locale>
      </schema>
      <xsl:text>

</xsl:text>
    </xsl:for-each>
    </schemalist>
  </gconfschemafile>
</xsl:template>

</xsl:stylesheet>
