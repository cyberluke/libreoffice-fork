<?xml version='1.0' encoding='UTF-8'?>
<description
  xmlns="http://openoffice.org/extensions/description/2006"
  xmlns:dep="http://openoffice.org/extensions/description/2006"
  xmlns:xlink="http://www.w3.org/1999/xlink"
  xmlns:d="http://openoffice.org/extensions/description/2006"
  xmlns:l="http://libreoffice.org/extensions/description/2011">
    <identifier value="org.extension.writeragent"/>
    <version value="{{VERSION}}"/>
	<dependencies>
		<l:LibreOffice-minimal-version d:name="LibreOffice 24.8" value="24.8"/>
	</dependencies>
    <publisher>
        <name xlink:href="https://officelabs.example.invalid/">OfficeLabs</name>
    </publisher>
    <display-name>
        <name>OfficeLabs AI</name>
    </display-name>

  <icon>
  <default xlink:href="assets/logo.jpg"/>
  </icon>

  <!--
    OfficeLabs overlay: the upstream GitHub self-update feed is intentionally
    removed. The bundled WriterAgent is owned and updated by the OfficeLabs
    application updater, never by the extension itself.
    See officelabs/writeragent/OFFICELABS_PATCHES.md.
  -->

</description>