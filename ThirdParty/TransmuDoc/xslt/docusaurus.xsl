<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="2.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:xs="http://www.w3.org/2001/XMLSchema"
	>
	
	<!-- 
						============= Some templates overrides specific to Docusaurus ================
	-->
	<xsl:template name="frontmatter">
		<xsl:param name="docid" as="xs:string?"/>
		<xsl:param name="slug" as="xs:string?"/>
		<xsl:param name="position" as="xs:integer?"/>
		<xsl:param name="title" as="xs:string?"/>
		<xsl:param name="description" as="xs:string?"/>
		<xsl:param name="sidebar" as="xs:string?"/>
		<xsl:param name="css-class" as="xs:string?"/>
		<xsl:text>---</xsl:text>
		<xsl:if test="$docid">
			<xsl:text>&#xA;id: </xsl:text><xsl:value-of select="$docid"/>
		</xsl:if>
		<xsl:if test="$slug">
			<xsl:text>&#xA;slug: </xsl:text><xsl:value-of select="$slug"/>
		</xsl:if>
		<xsl:if test="$title">
			<xsl:text>&#xA;title: </xsl:text><xsl:value-of select="$title"/>
		</xsl:if>
		<xsl:if test="$description">
			<xsl:text>&#xA;description: </xsl:text><xsl:value-of select="$description"/>
		</xsl:if>
		<xsl:if test="$sidebar">
			<xsl:text>&#xA;displayed_sidebar: </xsl:text><xsl:value-of select="$sidebar"/>
		</xsl:if>
		<xsl:if test="$position">
			<xsl:text>&#xA;sidebar_position: </xsl:text><xsl:value-of select="$position"/>
		</xsl:if>
		<xsl:if test="$css-class">
			<xsl:text>&#xA;sidebar_class_name: </xsl:text><xsl:value-of select="$css-class"/>
		</xsl:if>
		<xsl:text>&#xA;---&#xA;&#xA;</xsl:text>
	</xsl:template>
	
	
</xsl:stylesheet>
