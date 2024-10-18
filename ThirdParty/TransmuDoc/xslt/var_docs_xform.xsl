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
	<xsl:template match="root[doctype='variable']">
		<xsl:apply-templates select="doctype">
			<xsl:with-param name="expected" select="'variable'"/>
		</xsl:apply-templates>
		<xsl:call-template name="var_doc"/>
	</xsl:template>
	
	<!-- Template to create a node file -->
	<xsl:template name="var_doc">
		<xsl:call-template name="notification"/>
		
		<xsl:call-template name="frontmatter">
			<xsl:with-param name="slug">
				<xsl:text>/api/</xsl:text>
				<xsl:value-of select="class_id"/>
				<xsl:text>/</xsl:text>
				<xsl:value-of select="id"/>
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
					<xsl:value-of select="display_name"/>
				</item>
			</xsl:with-param>
		</xsl:call-template>
		
		<!-- Title of the wiki page -->
		<xsl:call-template name="title">
			<xsl:with-param name="title" select="display_name"/>
		</xsl:call-template>
		
		<!-- Details -->
		<xsl:text>&#xA;**Class:** </xsl:text>
		<xsl:call-template name="link">
			<xsl:with-param name="name" select="my:no_wrap(my:oneline(class_name))"/>
			<xsl:with-param name="href">
				<xsl:text>../../</xsl:text>
				<xsl:value-of select="class_id"/><xsl:text>.md</xsl:text>
			</xsl:with-param>
		</xsl:call-template>
		<xsl:text>\&#xA;**Category:** </xsl:text>
		<xsl:apply-templates select="my:no_wrap(my:oneline(category))"/>
		<xsl:text>\&#xA;**Type:** </xsl:text>
		<xsl:apply-templates select="my:no_wrap(my:oneline(variable_type))"/>
		<xsl:if test="editor_access">
			<xsl:text>\&#xA;**Editor Access:** </xsl:text>
			<xsl:apply-templates select="my:no_wrap(my:oneline(editor_access))"/>
		</xsl:if>
		<xsl:if test="blueprint_access">
			<xsl:text>\&#xA;**Blueprint Access:** </xsl:text>
			<xsl:apply-templates select="my:no_wrap(my:oneline(blueprint_access))"/>
		</xsl:if>
		<xsl:text>&#xA;</xsl:text>
		
		<xsl:apply-templates select="description"/>
		
		<xsl:text>&#xA;#### Nodes&#xA;&#xA;</xsl:text>
		<xsl:if test="imgpath_get">
			<xsl:call-template name="image">
				<xsl:with-param name="href" select="imgpath_get"/>
			</xsl:call-template>
		</xsl:if>
		<xsl:if test="imgpath_get and imgpath_set">
			<xsl:text> </xsl:text>
		</xsl:if>
		<xsl:if test="imgpath_set">
			<xsl:call-template name="image">
				<xsl:with-param name="href" select="imgpath_set"/>
			</xsl:call-template>
		</xsl:if>
		<xsl:text>&#xA;</xsl:text>
	</xsl:template>
	
	<!-- Unwanted elements (can use "a | b | c") -->
	<xsl:template match="fulltitle | docs_name | class_id | class_name"/>
</xsl:stylesheet>
