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
	<xsl:template match="root[doctype='class']">
		<xsl:apply-templates select="doctype">
			<xsl:with-param name="expected">class</xsl:with-param>
		</xsl:apply-templates>
		<xsl:call-template name="class_doc"/>
	</xsl:template>
	
	<!-- Redirect struct and enum docs to class template -->
	<xsl:template match="root[doctype=('struct', 'enum')]">
		<xsl:call-template name="class_doc"/>
	</xsl:template>
	
	<!-- Template to create a class file -->
	<xsl:template name="class_doc">
		<xsl:call-template name="notification"/>
		
		<!-- @TODO: Adding frontmatter ? -->
		
		<!-- Breadcrumb -->
		<xsl:call-template name="breadcrumb">
			<xsl:with-param name="items" as="element()*">
				<item href="../index.md">
					<xsl:value-of select="docs_name"/>
				</item>
				<item>
					<xsl:value-of select="display_name"/>
				</item>
			</xsl:with-param>
		</xsl:call-template>
		
		<!-- Title of the wiki page -->
		<xsl:call-template name="title">
			<xsl:with-param name="title" select="display_name"/>
		</xsl:call-template>
		
		<!-- Details -->
		<xsl:text>&#xA;## Class Details&#xA;</xsl:text>
		
		<xsl:apply-templates select="sourcepath"/>
		<xsl:if test='sourcepath and classTree'>
			<xsl:text>\&#xA;</xsl:text>
		</xsl:if>
		<xsl:apply-templates select="classTree"/>
		
		<xsl:text>&#xA;&#xA;</xsl:text>
		<xsl:apply-templates select="description"/>
		<xsl:text>&#xA;</xsl:text>
		
		<!-- Tables -->
		<xsl:apply-templates select="properties | fields[.//inherited='false']"/>
		<xsl:apply-templates select="nodes"/>
		<xsl:apply-templates select="values"/>
	</xsl:template>
	
	<xsl:template match="sourcepath">
		<xsl:text>**Defined in:** `</xsl:text>
		<xsl:value-of select="."/>
		<xsl:text>`</xsl:text>
	</xsl:template>
	
	<xsl:template match="classTree">
		<xsl:text>**Hierarchy:** *</xsl:text>
		<xsl:value-of select="."/>
		<xsl:text>*</xsl:text>
	</xsl:template>
	
	<!-- Template for the node table -->
	<xsl:template match="nodes">
		<xsl:text>&#xA;## Nodes&#xA;&#xA;</xsl:text>
		<xsl:text>| Name | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="node">
			<xsl:sort select="shorttitle"/>
		</xsl:apply-templates>
	</xsl:template>
	
	<!-- Template for a node row -->
	<xsl:template match="node">
		<xsl:text>| </xsl:text>
		<xsl:call-template name="link">
			<xsl:with-param name="name" select="shorttitle"/>
			<xsl:with-param name="href">
				<xsl:text>./nodes/</xsl:text><xsl:value-of select="id"/><xsl:text>.md</xsl:text>
			</xsl:with-param>
		</xsl:call-template>
		<xsl:text> | </xsl:text>
		<xsl:apply-templates select="description"/>
		<xsl:text> |&#xA;</xsl:text>
	</xsl:template>
	
	<!-- Template for enum values table -->
	<xsl:template match="values">
		<xsl:text>&#xA;## Values&#xA;&#xA;</xsl:text>
		<xsl:text>| Name | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="value"/>
	</xsl:template>
	
	<!-- Template for an enum value row -->
	<xsl:template match="value">
		<xsl:text>| </xsl:text>
		<xsl:value-of select="displayname"/>
		<xsl:text> | </xsl:text>
		<xsl:apply-templates select="description"/>
		<xsl:text> |&#xA;</xsl:text>
	</xsl:template>
	
</xsl:stylesheet>
