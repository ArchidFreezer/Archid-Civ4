set JAVA_HOME=E:\Apps\Java\jdk-8.0.341
set Path=%JAVA_HOME%\bin;%PATH%
java -cp civ4utils.jar org.archid.civ4.main.InfoUtils -f "./Civ4TechInfos.xml" -x "./Civ4TechInfos.xlsx" -t Tech -i 