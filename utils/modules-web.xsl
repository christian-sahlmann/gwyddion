<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml"
  encoding="utf-8"
  doctype-public='-//W3C//DTD XHTML 1.0 Strict//EN'
  doctype-system='http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd'/>

<xsl:template match="modulelist">
  <!--
    Part I.
    Global module info (like the upper part of module browser).
  -->
  <table>
    <thead><th>Name</th><th>Version</th><th>Authors</th></thead>
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
    <p>
      <b>Description: </b><xsl:apply-templates select="description"/>
    </p>
    <p>
      <b>Functions: </b>
      <xsl:for-each select="funclist/func">
        <xsl:if test="position() > 1">
          <xsl:text>, </xsl:text>
        </xsl:if>
        <xsl:value-of select="class"/>
        <xsl:text>::</xsl:text>
        <xsl:value-of select="name"/>
      </xsl:for-each>
    </p>
  </xsl:for-each>
</xsl:template>

</xsl:stylesheet>
