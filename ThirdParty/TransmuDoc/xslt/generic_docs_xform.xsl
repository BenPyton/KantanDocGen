<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0">
	<xsl:output
		method="text"
		encoding="utf-8"
		omit-xml-declaration="yes"
		indent="no"/>

	<!-- Include first the default root template, it will have the least precedence. -->
	<xsl:import href="no_xform.xsl"/>
	
	<!-- Include specific doc files -->
	<xsl:import href="index_xform.xsl"/>
	<xsl:import href="class_docs_xform.xsl"/>
	<xsl:import href="node_docs_xform.xsl"/>

	<!-- Choose the correct doc xsl based on the doctype -->
	<xsl:template match="/">
		<xsl:apply-templates/>
	</xsl:template>
</xsl:stylesheet>
