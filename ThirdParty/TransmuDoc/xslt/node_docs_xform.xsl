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
	<xsl:template match="root[doctype='node']">
		<xsl:apply-templates select="doctype">
			<xsl:with-param name="expected">node</xsl:with-param>
		</xsl:apply-templates>
		<xsl:call-template name="node_doc"/>
	</xsl:template>

	<!-- Template to create a node file -->
	<xsl:template name="node_doc">
		<xsl:call-template name="notification"/>
		
		<!-- Breadcrumb -->
		<xsl:call-template name="breadcrumb">
			<xsl:with-param name="items" as="element()*">
				<item>
					<xsl:call-template name="link">
						<xsl:with-param name="name" select="docs_name"/>
						<xsl:with-param name="href">
							<xsl:text>../../index.md</xsl:text>
						</xsl:with-param>
					</xsl:call-template>
				</item>
				<item>
					<xsl:call-template name="link">
						<xsl:with-param name="name" select="class_name"/>
						<xsl:with-param name="href">
							<xsl:text>../</xsl:text><xsl:value-of select="translate(class_name, ' ', '-')"/><xsl:text>.md</xsl:text>
						</xsl:with-param>
					</xsl:call-template>
				</item>
				<item>
					<xsl:value-of select="shorttitle"/>
				</item>
			</xsl:with-param>
		</xsl:call-template>

		<xsl:text>&#xA;Node&#xA;&#xA;</xsl:text>
		<xsl:text>![img](</xsl:text>
			<xsl:value-of select="imgpath"/>
		<xsl:text>)</xsl:text>
		<xsl:text>&#xA;&#xA;</xsl:text>
		<xsl:apply-templates select="rawsignature"/>
		<xsl:text>&#xA;</xsl:text>
		<xsl:apply-templates select="description"/>
		<xsl:apply-templates select="inputs"/>
		<xsl:apply-templates select="outputs"/>
	</xsl:template>

	<!-- Template for the input table -->
	<xsl:template match="inputs">
		<xsl:text>&#xA;## Inputs&#xA;&#xA;</xsl:text>
		<xsl:text>| Name | Type | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ---- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="param"/>
	</xsl:template>

	<!-- Template for the output table -->
	<xsl:template match="outputs">
		<xsl:text>&#xA;## Outputs&#xA;&#xA;</xsl:text>
		<xsl:text>| Name | Type | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ---- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="param"/>
	</xsl:template>

	<!-- Template for a param row -->
	<xsl:template match="param">
		<xsl:text>| </xsl:text>
			<xsl:value-of select="name"/>
		<xsl:text> | </xsl:text>
			<xsl:value-of select="type"/>
		<xsl:text> | </xsl:text>
			<xsl:apply-templates select="description"/>
		<xsl:text> |&#xA;</xsl:text>
	</xsl:template>
	
	<!-- Template for the C++ function signature -->
	<xsl:template match="rawsignature">
		<xsl:text>C++&#xA;```cpp&#xA;</xsl:text>
		<xsl:value-of select="."/>
		<xsl:text>&#xA;```</xsl:text>
	</xsl:template>

	<!-- Unwanted elements (can use "a | b | c") -->
	<xsl:template match="fulltitle | docs_name | class_id | class_name"/>
</xsl:stylesheet>
