@echo off
rem Writer 2027 - clean launcher.
rem 1) Kills ALL stale LibreOffice processes so the NEW build's DLLs always
rem    load (LibreOffice keeps a background soffice.bin alive; launching into
rem    it reuses the OLD code and reproduces the old crashes).
rem 2) Starts Writer in the Writer 2027 UI on a dedicated profile.
rem --norestore skips the document-recovery dialog after any hard kill.
taskkill /IM soffice.bin /F >nul 2>&1
taskkill /IM soffice.exe /F >nul 2>&1
taskkill /IM swriter.exe /F >nul 2>&1
timeout /t 2 /nobreak >nul
start "" "D:\_SATIN_AI\LibreOffice\instdir\program\soffice.exe" --norestore "--accept=socket,host=localhost,port=2102;urp;" -env:UserInstallation=file:///C:/Users/LUKES~1.COR/AppData/Local/Temp/kilo/loprof_w2027 %*