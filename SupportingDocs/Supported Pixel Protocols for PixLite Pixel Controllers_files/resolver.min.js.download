define(['underscore','domReady!'],function(_){'use strict';var context=require.s.contexts._,execCb=context.execCb,registry=context.registry,callbacks=[],retries=10,updateDelay=1,ready,update;function isSubscribed(callback){return!!_.findWhere(callbacks,callback);}
function isRejected(module){return registry[module.id]&&(registry[module.id].inited||registry[module.id].error);}
function isPending(module){if(!module.depCount){return false;}
return module.depCount>_.filter(module.depMaps,isRejected).length;}
function hasPending(){return _.some(registry,isPending);}
function isReady(){return ready&&!hasPending();}
function invoke(callback){callback.handler.call(callback.ctx);}
function resolve(){ready=true;callbacks.splice(0).forEach(invoke);}
function tick(){ready=false;update(retries);}
function subscribe(handler,ctx){var callback={handler:handler,ctx:ctx};if(!isSubscribed(callback)){callbacks.push(callback);if(isReady()){_.defer(tick);}}}
update=_.debounce(function(retry){if(!hasPending()){retry?update(--retry):resolve();}},updateDelay);context.execCb=function(){var exported=execCb.apply(context,arguments);tick();return exported;};return subscribe;});