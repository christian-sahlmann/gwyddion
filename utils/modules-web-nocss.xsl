<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml"
  encoding="utf-8"
  omit-xml-declaration="yes"/>

<xsl:template match="modulelist">
  <!--
    Part I.
    Global module info (like the upper part of module browser).
  -->
  <table>
    <thead><tr><th>Name</th><th>Version</th><th>Authors</th></tr></thead>
    <tbody>
    <xsl:for-each select="module">
    <tr>
      <td><a>
        <xsl:attribute name="href">#<xsl:value-of select="name"/></xsl:attribute>
        <xsl:value-of select="name"/>
      </a></td>
      <td><xsl:value-of select="version"/></td>
      <td><xsl:value-of select="author"/></td>
    </tr>
    </xsl:for-each>
    </tbody>
  </table>

  <!--
    Part II.
    Detailed module info (like the lower part of module browser).
  -->
  <xsl:for-each select="module">
    <h2>
      <xsl:attribute name="id"><xsl:value-of select="name"/></xsl:attribute>
      <xsl:apply-templates select="name"/>
    </h2>
    <p>
      <b>Version: </b><xsl:value-of select="version"/><br/>
      <b>Authors: </b><xsl:value-of select="author"/><br/>
      <b>Copyright: </b>
        <xsl:value-of select="copyright"/>
        <xsl:text> </xsl:text>
        <xsl:value-of select="date"/>
    </p>
    <p><b>Description: </b><xsl:apply-templates select="description"/>
      <xsl:if test="count(child::userguide) > 0">
        <xsl:text> (</xsl:text>
        <a>
          <xsl:attribute name="href">http://gwyddion.net/documentation/user-guide/<xsl:value-of select="userguide"/></xsl:attribute>
          <xsl:text>User guide</xsl:text>
        </a>
        <xsl:text>)</xsl:text>
      </xsl:if>
    </p>
    <xsl:if test="count(child::funclist/func) > 0">
    <table>
      <thead>
        <tr><th colspan="3">Functions</th></tr>
        <tr><th>Type</th><th>Name</th><th>Information</th></tr>
      </thead>
      <tbody>
      <xsl:for-each select="funclist/func">
        <tr>
          <td><xsl:value-of select="class"/></td>
          <td><xsl:value-of select="name"/></td>
          <td><xsl:value-of select="info"/></td>
        </tr>
      </xsl:for-each>
      </tbody>
    </table>
    </xsl:if>
  </xsl:for-each>
</xsl:template>

</xsl:stylesheet>
