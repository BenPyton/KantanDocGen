<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0">

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
			<xsl:if test="position() > 1">
				<xsl:text> &gt; </xsl:text> 
			</xsl:if>
			<xsl:value-of select="."/>
		</xsl:for-each>

		<xsl:text>&#xA;</xsl:text>
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
