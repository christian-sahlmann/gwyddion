<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml"
  encoding="utf-8"
  omit-xml-declaration="yes"/>

<xsl:template match="modulelist">
  <table class="modulelist">
    <thead>
      <tr>
        <th>
          <xsl:choose>
            <xsl:when test="$lang = 'en'">Name</xsl:when>
            <xsl:when test="$lang = 'fr'">Nom</xsl:when>
            <xsl:when test="$lang = 'ru'">Имя</xsl:when>
            <xsl:otherwise>Name</xsl:otherwise>
          </xsl:choose>
        </th>
        <th>
          <xsl:choose>
            <xsl:when test="$lang = 'en'">Version</xsl:when>
            <xsl:when test="$lang = 'fr'">Version</xsl:when>
            <xsl:when test="$lang = 'ru'">Версия</xsl:when>
            <xsl:otherwise>Version</xsl:otherwise>
          </xsl:choose>
        </th>
        <th>
          <xsl:choose>
            <xsl:when test="$lang = 'en'">Authors</xsl:when>
            <xsl:when test="$lang = 'fr'">Auteurs</xsl:when>
            <xsl:when test="$lang = 'ru'">Авторы</xsl:when>
            <xsl:otherwise>Authors</xsl:otherwise>
          </xsl:choose>
        </th>
      </tr>
    </thead>
    <tbody>
    <xsl:for-each select="module">
      <tr>
        <td>
          <div>
            <p>
              <b>
                <xsl:choose>
                  <xsl:when test="$lang = 'en'">Module</xsl:when>
                  <xsl:when test="$lang = 'fr'">Module</xsl:when>
                  <xsl:when test="$lang = 'ru'">Модуль</xsl:when>
                  <xsl:otherwise>Module</xsl:otherwise>
                </xsl:choose>
                <xsl:text>:&#32;</xsl:text>
              </b>
              <xsl:value-of select="name"/>-<xsl:value-of select="version"/>
              <br/>
              <xsl:text>© </xsl:text>
              <xsl:value-of select="copyright"/>
              <xsl:text> </xsl:text>
              <xsl:value-of select="date"/>
            </p>
            <p><b>
              <xsl:choose>
                <xsl:when test="$lang = 'en'">Description</xsl:when>
                <xsl:when test="$lang = 'fr'">Description</xsl:when>
                <xsl:when test="$lang = 'ru'">Описание</xsl:when>
                <xsl:otherwise>Description</xsl:otherwise>
              </xsl:choose>
              <xsl:text>:&#32;</xsl:text>
            </b><xsl:value-of select="description"/></p>
            <xsl:if test="count(child::funclist/func) > 0">
              <table>
                <thead>
                  <tr><th colspan="3">
                   <xsl:choose>
                     <xsl:when test="$lang = 'en'">Functions</xsl:when>
                     <xsl:when test="$lang = 'fr'">Fonctions</xsl:when>
                     <xsl:when test="$lang = 'ru'">Функции</xsl:when>
                     <xsl:otherwise>Functions</xsl:otherwise>
                   </xsl:choose>
                   <xsl:text>:</xsl:text>
                  </th></tr>
                  <tr>
                    <th>
                       <xsl:choose>
                         <xsl:when test="$lang = 'en'">Type</xsl:when>
                         <xsl:when test="$lang = 'fr'">Type</xsl:when>
                         <xsl:when test="$lang = 'ru'">Тип</xsl:when>
                         <xsl:otherwise>Type</xsl:otherwise>
                       </xsl:choose>
                    </th>
                    <th>
                       <xsl:choose>
                         <xsl:when test="$lang = 'en'">Name</xsl:when>
                         <xsl:when test="$lang = 'fr'">Nom</xsl:when>
                         <xsl:when test="$lang = 'ru'">Имя</xsl:when>
                         <xsl:otherwise>Name</xsl:otherwise>
                       </xsl:choose>
                    </th>
                    <th>
                       <xsl:choose>
                         <xsl:when test="$lang = 'en'">Information</xsl:when>
                         <xsl:when test="$lang = 'fr'">Information</xsl:when>
                         <xsl:when test="$lang = 'ru'">Информация</xsl:when>
                         <xsl:otherwise>Information</xsl:otherwise>
                       </xsl:choose>
                    </th>
                  </tr>
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
          <p>
            <xsl:choose>
              <xsl:when test="count(child::userguide) > 0">
                <a><xsl:attribute name="href">http://gwyddion.net/documentation/user-guide-<xsl:value-of select="$lang"/>/<xsl:value-of select="userguide"/></xsl:attribute>
                  <xsl:value-of select="name"/>
                </a>
              </xsl:when>
              <xsl:otherwise>
                <xsl:value-of select="name"/>
              </xsl:otherwise>
            </xsl:choose>
          </p>
        </td>
        <td><xsl:value-of select="version"/></td>
        <td><xsl:value-of select="author"/></td>
      </tr>
    </xsl:for-each>
    </tbody>
  </table>
</xsl:template>

</xsl:stylesheet>
