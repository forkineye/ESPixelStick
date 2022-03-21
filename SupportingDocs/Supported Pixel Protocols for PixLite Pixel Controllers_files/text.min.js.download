define(['module'],function(module){'use strict';var xmlRegExp=/^\s*<\?xml(\s)+version=[\'\"](\d)*.(\d)*[\'\"](\s)*\?>/im,bodyRegExp=/<body[^>]*>\s*([\s\S]+)\s*<\/body>/im,stripReg=/!strip$/i,defaultConfig=module.config&&module.config()||{};function stripContent(external){var matches;if(!external){return'';}
matches=external.match(bodyRegExp);external=matches?matches[1]:external.replace(xmlRegExp,'');return external;}
function sameDomain(url){var uProtocol,uHostName,uPort,xdRegExp=/^([\w:]+)?\/\/([^\/\\]+)/i,location=window.location,match=xdRegExp.exec(url);if(!match){return true;}
uProtocol=match[1];uHostName=match[2];uHostName=uHostName.split(':');uPort=uHostName[1]||'';uHostName=uHostName[0];return(!uProtocol||uProtocol===location.protocol)&&(!uHostName||uHostName.toLowerCase()===location.hostname.toLowerCase())&&(!uPort&&!uHostName||uPort===location.port);}
function createRequest(url){var xhr=new XMLHttpRequest();if(!sameDomain(url)&&typeof XDomainRequest!=='undefined'){xhr=new XDomainRequest();}
return xhr;}
function getContent(url,callback,fail,headers){var xhr=createRequest(url),header;xhr.open('GET',url);if('setRequestHeader'in xhr&&headers){for(header in headers){if(headers.hasOwnProperty(header)){xhr.setRequestHeader(header.toLowerCase(),headers[header]);}}}
xhr.onreadystatechange=function(){var status,err;if(xhr.readyState===4){status=xhr.status||0;if(status>399&&status<600){err=new Error(url+' HTTP status: '+status);err.xhr=xhr;if(fail){fail(err);}}else{callback(xhr.responseText);if(defaultConfig.onXhrComplete){defaultConfig.onXhrComplete(xhr,url);}}}};if(defaultConfig.onXhr){defaultConfig.onXhr(xhr,url);}
xhr.send();}
function loadContent(name,req,onLoad){var toStrip=stripReg.test(name),url=req.toUrl(name.replace(stripReg,'')),headers=defaultConfig.headers;getContent(url,function(content){content=toStrip?stripContent(content):content;onLoad(content);},onLoad.error,headers);}
return{load:loadContent,get:getContent};});