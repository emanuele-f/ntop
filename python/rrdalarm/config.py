'''
Created on 16/gen/2010

@author: Gianluca Medici
'''

import ntop
import host
import os
import sys
#import pprint

# Import modules for CGI handling
import cgi, cgitb

from StringIO import StringIO

def begin():
    
    templateFilename='rrdAlarmConfigurator.tmpl'
    # Imports for mako
    try:
        from mako.template import Template
        from mako.runtime import Context
        from mako.lookup import TemplateLookup
        from mako import exceptions
    except:
        ntop.printHTMLHeader('ntop Python Configuration Error',1,0)
        ntop.sendString("<b><center><font color=red>Please install <A HREF=http://www.makotemplates.org/>Mako</A> template engine</font> (sudo easy_install Mako)</center></b>")
        ntop.printHTMLFooter()    
        return
    
    # Fix encoding
    reload(sys)
    sys.setdefaultencoding("latin1")
    
    
    rows=[]
    pathRRDFiles=ntop.getDBPath()+'/'
    nameFileConfig='rrdAlarmConfig.txt'              #default nameFileConfig
    pathTempFile=ntop.getSpoolPath()+'/'
    #nameFileConfig='/home/gianluca/rrdAlarmConfig.txt'
    
    form = cgi.FieldStorage();                      #get  from the url the parameter configfile that contains the 
                                                    #path+filename of the configfile to read
                                                    
    help=form.getvalue('help')
    
    if help == 'true':
        #show help page
        templateFilename='rrdAlarmConfiguratorHelp.tmpl'
        ntop.printHTMLHeader('RRD Alarm Configurator Help', 1, 0)
    else:
        requestFileConfig=form.getvalue('configFile')
        if  requestFileConfig != None:
            nameFileConfig=requestFileConfig
        
        ntop.printHTMLHeader('RRD Alarm Configurator', 1, 0)
        
            
        try:
            configFile= open((pathTempFile+nameFileConfig), 'rt')
            
            for line in configFile:
                line=line.rstrip()                      #drop the \n at the end
                if len(line) >0 and line[0] != '#':
                    rows.append(line.split('\t'))
            
            configFile.close()
        except IOError:
            if requestFileConfig != None :                 #if the nameFileConfig was specified by user show error
                nameFileConfig='/rrdAlarmConfig.txt'
                ntop.sendString(exceptions.html_error_template().render())
    
    try:
        documentRoot=os.getenv('DOCUMENT_ROOT', '.')
        basedir =  documentRoot+'/python/templates'
        mylookup = TemplateLookup(directories=[basedir])
        myTemplate = mylookup.get_template(templateFilename)
        buf = StringIO()
        ctx=None
        if(help =='true'):          #show help page
            ctx = Context(buf)
        else:
            ctx = Context(buf, configRows=rows,tempFilePath=pathTempFile, nameFileConfig=nameFileConfig, pathRRDFiles=pathRRDFiles)
        
        myTemplate.render_context(ctx)
        ntop.sendString(buf.getvalue())
    except:
        ntop.sendString(exceptions.html_error_template().render())
    
    ntop.printHTMLFooter()


'''Here starts the script'''

begin()