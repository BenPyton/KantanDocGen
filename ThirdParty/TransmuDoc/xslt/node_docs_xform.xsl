<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="2.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:my="http://benpyton.github.io/TransmuDoc"
	>
	
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
			<xsl:with-param name="expected" select="'node'"/>
		</xsl:apply-templates>
		<xsl:call-template name="node_doc"/>
	</xsl:template>
	
	<!-- Template to create a node file -->
	<xsl:template name="node_doc">
		<xsl:call-template name="notification"/>
		
		<xsl:call-template name="frontmatter">
			<xsl:with-param name="slug">
				<xsl:text>/api/</xsl:text>
				<xsl:value-of select="class_id"/>
				<xsl:text>/</xsl:text>
				<xsl:value-of select="funcname"/>
			</xsl:with-param>
		</xsl:call-template>
		
		<!-- Breadcrumb -->
		<xsl:call-template name="breadcrumb">
			<xsl:with-param name="items" as="element()*">
				<item href="../../../../index.md">
					<xsl:value-of select="docs_name"/>
				</item>
				<item>
					<xsl:attribute name="href">
						<xsl:text>../../</xsl:text>
						<xsl:value-of select="class_id"/><xsl:text>.md</xsl:text>
					</xsl:attribute>
					<xsl:value-of select="class_name"/>
				</item>
				<item>
					<xsl:value-of select="shorttitle"/>
				</item>
			</xsl:with-param>
		</xsl:call-template>
		
		<!-- Title of the wiki page -->
		<xsl:call-template name="title">
			<xsl:with-param name="title" select="shorttitle"/>
		</xsl:call-template>
		
		<!-- Details -->
		<xsl:text>&#xA;&#xA;**Category:** </xsl:text>
		<xsl:apply-templates select="my:no_wrap(my:oneline(category))"/>
		<xsl:text>&#xA;</xsl:text>
		<xsl:apply-templates select="description"/>
		
		<xsl:text>&#xA;Node&#xA;&#xA;</xsl:text>
		<xsl:call-template name="image">
			<xsl:with-param name="href" select="imgpath"/>
		</xsl:call-template>
		<xsl:text>&#xA;&#xA;</xsl:text>
		
		<xsl:apply-templates select="rawsignature"/>
		
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
