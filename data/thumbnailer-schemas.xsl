<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		            xmlns:m='http://www.freedesktop.org/standards/shared-mime-info'>
<xsl:output method="xml"
            encoding="utf-8"
            indent="yes"/>

<xsl:template match="m:mime-info">
  <!-- Keep GENERATED quoted to prevent treating *this* file as generated. -->
  <xsl:comment> This is a <xsl:text>GENERATED</xsl:text> file. </xsl:comment>
  <gconfschemafile>
    <schemalist>
    <xsl:for-each select="m:mime-type">
      <schema>
        <key>/schemas/desktop/gnome/thumbnailers/<xsl:value-of select="translate(@type,'/','@')"/>/enable</key>
        <applyto>/schemas/desktop/gnome/thumbnailers/<xsl:value-of select="translate(@type,'/','@')"/>/enable</applyto>
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
        <applyto>/schemas/desktop/gnome/thumbnailers/<xsl:value-of select="translate(@type,'/','@')"/>/command</applyto>
        <owner>gwyddion</owner>
        <type>string</type>
        <default>gwyddion-thumbnailer gnome2 %s %i %o</default>
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
