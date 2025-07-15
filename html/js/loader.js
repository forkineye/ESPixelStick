let done = false;
let retry = 0;

async function loadCss(href, cssId)
{
    // console.debug("loadCss: Start. href: " + href);

    done = false;
    retry ++;

    var head  = document.getElementsByTagName('head')[0];
    var link  = document.createElement('link');
    link.id   = cssId;
    link.rel  = 'stylesheet';
    link.type = 'text/css';
    link.href = href;
    link.media = 'all';
    link.onload = function()
    {
        done = true;
        retry = 0;
    };
    link.onerror = function()
    {
        // console.debug("loadCss: retry href: " + href);
        if(3 > retry)
        {
            const linkElement = document.querySelector('link[href="' + href + '"]');
            if (linkElement)
            {
                linkElement.remove();
            }
            loadCss(href, cssId);
        }
        else
        {
            // give up
            // console.debug("loadCss: retry failed:");
            done = true;
            retry = 0;
        }
    };
    head.appendChild(link);

    await WaitForLoadDone();

    // console.debug("loadCss: End. done:" + done);
} // loadCss

async function loadScript(src)
{
    // console.debug("loadScript: Start. src: " + src);

    done = false;
    retry ++;

    const script = document.createElement('script');
    script.src = src;
    script.async = false;
    script.onload = function()
    {
        done = true;
        retry = 0;
    };
    script.onerror = function()
    {
        if(3 > retry)
        {
            // console.debug("loadScript: retry src: " + src);
            const scriptElement = document.querySelector('script[src="' + src + '"]');
            if (scriptElement && scriptElement.parentNode)
            {
                scriptElement.parentNode.removeChild(scriptElement);
            }
            loadScript(src);
        }
        else
        {
            // give up
            // console.debug("loadScript: retry failed:");
            done = true;
            retry = 0;
        }
    };
    document.head.appendChild(script);

    await WaitForLoadDone();
    // console.debug("loadScript: End. done:" + done);

} // loadScript

async function WaitForLoadDone()
{
    // console.debug("WaitForLoadDone: Start. done:" + done);
    while (!done)
    {
        // console.debug("WaitForLoadDone: Start waiting")
        await new Promise(resolve => setTimeout(resolve, 100)); // Poll every 100ms
    }
    // console.debug("WaitForLoadDone: End. done:" + done);
}// WaitForLoadDone

async function main()
{
    await loadCss("css/bootstrap.css", "bootstrapStyles");
    await loadCss("css/style.css", "genericStyles");
    await loadCss("css/dropzone.css", "dropzone_styles");

    await loadScript("js/jquery-3.7.1.js");
    await loadScript("js/bootstrap.js");
    await loadScript("js/jqColorPicker.js");
    await loadScript("js/jquery.cookie.js");
    await loadScript("js/dropzone.js");
    await loadScript("js/FileSaver.js");
    await loadScript("js/script.js");
} // main

// kick off the load sequence
main();
