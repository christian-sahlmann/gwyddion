<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml"
  encoding="utf-8"
  omit-xml-declaration="yes"/>

<xsl:template match="modulelist">
  <table class="modulelist">
    <thead><tr><th>Name</th><th>Version</th><th>Authors</th></tr></thead>
    <tbody>
    <xsl:for-each select="module">
      <tr>
        <td>
          <div>
            <p>
              <b>Module: </b>
              <xsl:value-of select="name"/>-<xsl:value-of select="version"/>
              <br/>
              <xsl:text>© </xsl:text>
              <xsl:value-of select="copyright"/>
              <xsl:text> </xsl:text>
              <xsl:value-of select="date"/>
            </p>
            <p><b>Description: </b><xsl:value-of select="description"/></p>
            <xsl:if test="count(child::funclist/func) > 0">
              <table>
                <thead>
                  <tr><th colspan="3">Functions:</th></tr>
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
          </div>
          <p><xsl:value-of select="name"/></p>
        </td>
        <td><xsl:value-of select="version"/></td>
        <td><xsl:value-of select="author"/></td>
      </tr>
    </xsl:for-each>
    </tbody>
  </table>
</xsl:template>

</xsl:stylesheet>
