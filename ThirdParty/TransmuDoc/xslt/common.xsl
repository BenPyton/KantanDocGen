<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="2.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:xs="http://www.w3.org/2001/XMLSchema"
	xmlns:my="http://benpyton.github.io/TransmuDoc"
	>
	
	<xsl:key name="class-by-id" match="class" use="id" />
	<xsl:key name="variable-by-id" match="variable" use="id" />

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
		<xsl:text>| Name | Type | Category | Accessors | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | ---- | -------- | --------- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="property | field[not(inheritedFrom) or key('class-by-id', inheritedFrom/id, document('../../index.xml',.))]">
			<xsl:sort select="inheritedFrom/id"/>
			<xsl:sort select="category"/>
			<xsl:sort select="display_name"/>
		</xsl:apply-templates>
	</xsl:template>
	
	<!-- Template for the property row -->
	<xsl:template match="property|field">
		<xsl:text>| </xsl:text>
		<xsl:choose>
			<xsl:when test="../../variables and key('variable-by-id', name, ../../variables)">
				<xsl:call-template name="link">
					<xsl:with-param name="name" select="display_name"/>
					<xsl:with-param name="href">
						<xsl:text>./Variables/</xsl:text>
						<xsl:value-of select="name"/>
						<xsl:text>/</xsl:text>
						<xsl:value-of select="name"/>
						<xsl:text>.md</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="display_name"/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if test="inheritedFrom">
			<xsl:text>&lt;br/&gt;(inherited from </xsl:text>
			<xsl:apply-templates select="inheritedFrom/display_name"/>
			<xsl:text>)</xsl:text>
		</xsl:if>
		<xsl:text> | </xsl:text>
		<xsl:value-of select="my:format(type)"/>
		<xsl:text> | </xsl:text>
		<xsl:value-of select="my:no_wrap(my:multiline(category))"/>
		<xsl:text> | </xsl:text>
		<xsl:apply-templates select="blueprint_access"/>
		<xsl:if test="blueprint_access and editor_access">
			<xsl:text>&lt;br/&gt;</xsl:text>
		</xsl:if>
		<xsl:apply-templates select="editor_access"/>
		<xsl:text> | </xsl:text>
		<xsl:apply-templates select="description"/>
		<xsl:text> |&#xA;</xsl:text>
	</xsl:template>

	<!-- Template for the event table -->
	<xsl:template match="events">
		<xsl:param name="name"/>
		<xsl:text>&#xA;## </xsl:text>
		<xsl:value-of select="if ($name) then $name else 'Events'"/>
		<xsl:text>&#xA;&#xA;</xsl:text>
		<xsl:text>| Name | Category | Description |&#xA;</xsl:text>
		<xsl:text>| ---- | -------- | ----------- |&#xA;</xsl:text>
		<xsl:apply-templates select="event[not(inheritedFrom) or key('class-by-id', inheritedFrom/id, document('../../index.xml',.))]">
			<xsl:sort select="inheritedFrom/id"/>
			<xsl:sort select="category"/>
			<xsl:sort select="display_name"/>
		</xsl:apply-templates>
	</xsl:template>

	<!-- Template for the event row -->
	<xsl:template match="event">
		<xsl:text>| </xsl:text>
		<xsl:apply-templates select="display_name"/>
		<xsl:if test="inheritedFrom">
			<xsl:text>&lt;br/&gt;(inherited from </xsl:text>
			<xsl:apply-templates select="inheritedFrom/display_name"/>
			<xsl:text>)</xsl:text>
		</xsl:if>
		<xsl:text> | </xsl:text>
		<xsl:value-of select="my:no_wrap(my:multiline(category))"/>
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
					<xsl:text> &#x23F5;&#xA;</xsl:text> <!-- Another candidate: &#x276F; -->
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
		<xsl:param name="docid" as="xs:string?"/>
		<xsl:param name="slug" as="xs:string?"/>
		<xsl:param name="position" as="xs:integer?"/>
		<xsl:param name="title" as="xs:integer?"/>
		<xsl:param name="sidebar" as="xs:integer?"/>
		<xsl:param name="description" as="xs:integer?"/>
		<xsl:param name="css-class" as="xs:string?"/>
	</xsl:template>
	
	<!-- Template to create a title in markdown -->
	<xsl:template name="title">
		<xsl:param name="title"/>
		<xsl:text>&#xA;# </xsl:text><xsl:value-of select="$title"/>
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
		<xsl:value-of select="my:format(.)"/>
	</xsl:template>
	
	<!-- Template for top-level description of a page -->
	<xsl:template match="/root/description">
		<xsl:if test=". != ''">
			<xsl:text>&#xA;## Description&#xA;&#xA;</xsl:text>
			<xsl:call-template name="formatted-description"/>
			<xsl:text>&#xA;</xsl:text>
		</xsl:if>
	</xsl:template>
	
	<!-- Template for classes derivable in blueprint -->
	<xsl:template match="blueprintable">
		<xsl:if test=". = 'true'">
			<xsl:text>Blueprint&amp;nbsp;Base&amp;nbsp;Class</xsl:text>
		</xsl:if>
	</xsl:template>
	
	<!-- Template for types usable as variable types in blueprint -->
	<xsl:template match="blueprint_type">
		<xsl:if test=". = 'true'">
			<xsl:text>Variable&amp;nbsp;Type</xsl:text>
		</xsl:if>
	</xsl:template>
	
	<!-- Template for properties blueprint access -->
	<xsl:template match="blueprint_access">
		<xsl:value-of select="my:no_wrap(concat('Blueprint ', .))"/>
	</xsl:template>
	
	<!-- Template for properties editor access -->
	<xsl:template match="editor_access">
		<xsl:value-of select="my:no_wrap(concat('Edit ', .))"/>
	</xsl:template>
	
	<!-- Template for class/node/variable displayed name -->
	<xsl:template match="display_name">
		<xsl:value-of select="my:no_wrap(my:format(.))"/>
	</xsl:template>
	
	<xsl:function name="my:no_wrap" as="xs:string">
		<xsl:param name="input" as="xs:string?"/>
		<xsl:value-of select="replace($input, ' ', '&amp;nbsp;')"/>
	</xsl:function>
	
	<xsl:function name="my:format" as="xs:string">
		<xsl:param name="input" as="xs:string?"/>
		<xsl:value-of select="
			replace(
				replace(
					replace($input
						, '&lt;', '&amp;lt;'
					)
					, '&gt;', '&amp;gt;'
				)
				, '&#xA;', '&lt;br/&gt;'
			)"/>
	</xsl:function>
	
	<xsl:function name="my:oneline" as="xs:string">
		<xsl:param name="input" as="xs:string?"/>
		<xsl:value-of select="replace($input, '\|', ' &amp;#8594; ')"/>
	</xsl:function>
	
	<xsl:function name="my:multiline" as="xs:string">
		<xsl:param name="input" as="xs:string?"/>
		<xsl:value-of select="replace($input, '\|', ' &lt;br/&gt;&#x2514; ')"/>
	</xsl:function>
	
</xsl:stylesheet>
