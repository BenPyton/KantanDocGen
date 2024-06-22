<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="2.0">

	<!-- This should be the root template with the least precedence (a.k.a imported first) -->
	<xsl:template match="root">
		<xsl:message>Error: No xsl:template for doc type: <xsl:value-of select="doctype"/></xsl:message>
	</xsl:template>
</xsl:stylesheet>
