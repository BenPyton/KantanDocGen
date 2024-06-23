<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0">
	<xsl:output
		method="text"
		encoding="utf-8"
		omit-xml-declaration="yes"
		indent="no"/>

	<!-- Include some common templates -->
	<xsl:import href="common.xsl"/>

	<!-- Root template -->
	<xsl:template match="/">
		<xsl:apply-templates select="root"/>
	</xsl:template>
	
	<!-- Template to match automatically based on doc_type -->
	<xsl:template match="root[doctype='index']">
		<xsl:apply-templates select="doctype">
			<xsl:with-param name="expected">index</xsl:with-param>
		</xsl:apply-templates>
		<xsl:call-template name="index_doc"/>
	</xsl:template>
	
	<!-- Templates to create an index file -->
	<xsl:template name="index_doc">
		<xsl:call-template name="notification"/>
		
		<xsl:text># </xsl:text><xsl:value-of select="display_name"/><xsl:text> API&#xA;</xsl:text>
		<xsl:apply-templates select="classes"/>
		<xsl:apply-templates select="structs"/>
		<xsl:apply-templates select="enums"/>
	</xsl:template>

	<!-- Template for the class table -->
	<xsl:template match="classes">
		<xsl:text>&#xA;## Classes&#xA;&#xA;</xsl:text>
		<xsl:text>| Type | Name | Group | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ---- | ----- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="class">
			<xsl:sort select="type" order="descending"/>
			<xsl:sort select="display_name"/>
		</xsl:apply-templates>
	</xsl:template>
	
	<!-- Template for a class row -->
	<xsl:template match="class">
		<xsl:text>| </xsl:text>
			<xsl:apply-templates select="type"/>
		<xsl:text> | </xsl:text>
			<xsl:call-template name="link">
				<xsl:with-param name="name" select="display_name"/>
				<xsl:with-param name="href">
					<xsl:text>./</xsl:text><xsl:value-of select="id"/>/<xsl:value-of select="translate(display_name, ' ', '-')"/><xsl:text>.md</xsl:text>
				</xsl:with-param>
			</xsl:call-template>
		<xsl:text> | </xsl:text>
			<xsl:apply-templates select="group"/>
		<xsl:text> | </xsl:text>
			<xsl:apply-templates select="description"/>
		<xsl:text> |&#xA;</xsl:text>
	</xsl:template>

	<!-- Template for the struct table -->
	<xsl:template match="structs">
		<xsl:text>&#xA;## Structs&#xA;&#xA;</xsl:text>
		<xsl:text>| Type | Name | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ---- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="struct">
			<xsl:sort select="display_name"/>
		</xsl:apply-templates>
	</xsl:template>

	<!-- Template for a struct row -->
	<xsl:template match="struct">
		<xsl:text>| </xsl:text>
			<xsl:apply-templates select="type"/>
		<xsl:text> | </xsl:text>
			<xsl:call-template name="link">
				<xsl:with-param name="name" select="display_name"/>
				<xsl:with-param name="href">
					<xsl:text>./</xsl:text><xsl:value-of select="id"/>/<xsl:value-of select="translate(display_name, ' ', '-')"/><xsl:text>.md</xsl:text>
				</xsl:with-param>
			</xsl:call-template>
		<xsl:text> | </xsl:text>
			<xsl:apply-templates select="description"/>
		<xsl:text> |&#xA;</xsl:text>
	</xsl:template>

	<!-- Template for the enum table -->
	<xsl:template match="enums">
		<xsl:text>&#xA;## Enums&#xA;&#xA;</xsl:text>
		<xsl:text>| Type | Name | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ---- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="enum">
			<xsl:sort select="display_name"/>
		</xsl:apply-templates>
	</xsl:template>

	<!-- Template for an enum row -->
	<xsl:template match="enum">
		<xsl:text>| </xsl:text>
			<xsl:apply-templates select="type"/>
		<xsl:text> | </xsl:text>
			<xsl:call-template name="link">
				<xsl:with-param name="name" select="display_name"/>
				<xsl:with-param name="href">
					<xsl:text>./</xsl:text><xsl:value-of select="id"/>/<xsl:value-of select="translate(display_name, ' ', '-')"/><xsl:text>.md</xsl:text>
				</xsl:with-param>
			</xsl:call-template>
		<xsl:text> | </xsl:text>
			<xsl:apply-templates select="description"/>
		<xsl:text> |&#xA;</xsl:text>
	</xsl:template>

</xsl:stylesheet>
