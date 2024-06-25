<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="2.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:xs="http://www.w3.org/2001/XMLSchema"
	>
	
	<!-- 
						============= Some templates overrides specific to Docusaurus ================
	-->
	<xsl:template name="frontmatter">
		<xsl:param name="slug" as="xs:string?"/>
		<xsl:param name="position" as="xs:integer?"/>
		<xsl:text>---&#xA;</xsl:text>
		<xsl:if test="$slug">
			<xsl:text>slug: </xsl:text><xsl:value-of select="$slug"/>
		</xsl:if>
		<xsl:if test="$position">
			<xsl:text>sidebar_position: </xsl:text><xsl:value-of select="$position"/>
		</xsl:if>
		<xsl:text>&#xA;---&#xA;&#xA;</xsl:text>
	</xsl:template>
	
	
</xsl:stylesheet>
