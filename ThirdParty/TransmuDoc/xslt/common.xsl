<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="2.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:xs="http://www.w3.org/2001/XMLSchema"
	>
	
	<!-- Notify file transformation -->
	<xsl:template name="notification">
		<xsl:message>
			<xsl:text>Transforming </xsl:text>
			<xsl:value-of select="./doctype"/>
			<xsl:text> </xsl:text>
			<xsl:value-of select="./display_name"/>
			<xsl:value-of select="./shorttitle"/>
		</xsl:message>
	</xsl:template>
	
	<!-- Template for the property table -->
	<xsl:template match="doctype">
		<xsl:param name="expected"/>
		<xsl:if test=". != $expected">
			<xsl:message>Error: doctype is not <xsl:value-of select="$expected"/> (<xsl:value-of select="."/>).</xsl:message>
		</xsl:if>
	</xsl:template>
	
	<!-- Template for the property table -->
	<xsl:template match="properties|fields">
		<xsl:param name="name"/>
		<xsl:text>&#xA;## </xsl:text>
		<xsl:value-of select="if ($name) then $name else 'Properties'"/>
		<xsl:text>&#xA;&#xA;</xsl:text>
		<xsl:text>| Name | Type | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ---- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="property | field[inherited='false']">
			<xsl:sort select="display_name"/>
		</xsl:apply-templates>
	</xsl:template>
	
	<!-- Template for the property row -->
	<xsl:template match="property|field">
		<xsl:text>| </xsl:text>
		<xsl:value-of select="display_name"/>
		<xsl:text> | </xsl:text>
		<xsl:value-of select="replace(replace(type, '&lt;', '&amp;lt;'), '&gt;', '&amp;gt;')" disable-output-escaping="yes"/>
		<xsl:text> | </xsl:text>
		<xsl:apply-templates select="description"/>
		<xsl:text> |&#xA;</xsl:text>
	</xsl:template>
	
	<!-- Template to create a breadcrumb from a list -->
	<xsl:template name="breadcrumb">
		<xsl:param name="items" as="element()*"/>
		<xsl:for-each select="$items">
			<xsl:choose>
				<xsl:when test="position() = last()">
					<xsl:call-template name="breadcrumb-item-last"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="breadcrumb-item"/>
					<xsl:text> &gt;&#xA;</xsl:text> 
				</xsl:otherwise>
			</xsl:choose>
		</xsl:for-each>
		<xsl:text>&#xA;</xsl:text>
	</xsl:template>
	
	<xsl:template name="breadcrumb-item">
		<xsl:call-template name="link">
			<xsl:with-param name="name" select="text()"/>
			<xsl:with-param name="href" select="@href"/>
		</xsl:call-template>
	</xsl:template>
	
	<xsl:template name="breadcrumb-item-last">
		<xsl:value-of select="."/>
	</xsl:template>
	
	<!-- Template to create a front matter at start of a markdown file -->
	<!-- Empty by default, should be overwritten by another import -->
	<xsl:template name="frontmatter">
		<xsl:param name="slug" as="xs:string?"/>
		<xsl:param name="position" as="xs:integer?"/>
	</xsl:template>
	
	<!-- Template to create a title in markdown -->
	<xsl:template name="title">
		<xsl:param name="title"/>
		<xsl:text>&#xA;# </xsl:text><xsl:value-of select="$title"/>
		<xsl:text>&#xA;&#xA;</xsl:text>
	</xsl:template>
	
	<!-- Template to create a link in markdown -->
	<xsl:template name="link">
		<xsl:param name="name"/>
		<xsl:param name="href"/>
		<xsl:text>[</xsl:text>
		<xsl:value-of select="$name"/>
		<xsl:text>](</xsl:text>
		<xsl:value-of select="$href"/>
		<xsl:text>)</xsl:text>
	</xsl:template>
	
	<!-- Template to show an image in markdown -->
	<xsl:template name="image">
		<xsl:param name="alt"/>
		<xsl:param name="href"/>
		<xsl:text>![</xsl:text>
		<xsl:value-of select="if ($alt) then $alt else ''"/>
		<xsl:text>](</xsl:text>
		<xsl:value-of select="$href"/>
		<xsl:text>)</xsl:text>
	</xsl:template>
	
	<!-- Template to format multiline description for markdown (mainly useful in tables) -->
	<xsl:template match="description" name="formatted-description">
		<xsl:value-of select="replace(.,'&#xA;', '&lt;br/&gt;')"/>
	</xsl:template>
	
	<!-- Template for top-level description of a page -->
	<xsl:template match="/root/description">
		<xsl:if test=". != ''">
			<xsl:text>&#xA;## Description&#xA;&#xA;</xsl:text>
			<xsl:call-template name="formatted-description"/>
			<xsl:text>&#xA;</xsl:text>
		</xsl:if>
	</xsl:template>
	
</xsl:stylesheet>
