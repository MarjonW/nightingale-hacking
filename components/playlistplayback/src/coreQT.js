/*
//
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyrightę 2006 POTI, Inc.
// http://songbirdnest.com
// 
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the "GPL").
// 
// Software distributed under the License is distributed 
// on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either 
// express or implied. See the GPL for the specific language 
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this 
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc., 
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
// 
// END SONGBIRD GPL
//
 */

/**
 * \file coreQT.js
 * \brief The CoreWrapper implementation for the QuickTime Plugin
 * \sa sbICoreWrapper.idl coreBase.js
 */

/**
 * \class CoreQT
 * \brief The CoreWrapper for the QuickTime Plugin
 * \sa CoreBase
 */
function CoreQT()
{
  this._sourcewindow = null;
  this._object = null;
  this._url = "";
  this._id = "";
  this._isremote = false;
  this._buffering = false;
  this._playing = false;
  this._paused  = false;
  this._availability = 0;
  this._waitForPlayInterval = null;
  this._waitRetryCount = 0;
  this._lastVolume = 0;
  this._muted = false;
  this._starttime = 0;
  this._playFromBeginning = true;
  
  this._pokingUrl = false;
  this._request = null;
  
  this.NOT_LOCAL_FILE = -1;
  this.FILE_NOT_AVAILABLE = 0;
  this.LOCAL_FILE_AVAILABLE = 1;
  this.REMOTE_FILE_AVAILABLE = 2;
};

// inherit the prototype from CoreBase
CoreQT.prototype = new CoreBase();

// set the constructor so we use ours and not the one for CoreBase
CoreQT.prototype.constructor = CoreQT();

CoreQT.prototype.cleanupOnError = function ()
{
  try {
    //Probably an error :(
    this._buffering = false;
    this._playing = false;
    this._paused = false;
    
    clearInterval(this._waitForPlayInterval);
    this._waitForPlayInterval = 0;
    this._waitRetryCount = 0;
    
    //XXXAus: Don't ask me why I have to do this twice, I DO NOT KNOW! =P
    this._sourcewindow.location.reload(false);
    SetQTObject();
    this._sourcewindow.location.reload(false);
    SetQTObject();
    
    this.LOG("cleanupOnError!", "CoreQT");

  } catch(e) {
    dump(e);
  }
}

CoreQT.prototype.waitForPlayer = function ()
{
  this._verifyObject();
  var playerStatus = this._object.GetPluginStatus();
  
  switch(playerStatus)
  {
    case "Waiting":
      this._waitRetryCount++;
      if(this._waitRetryCount > 200) {
        this.cleanupOnError();
      }
    break;
    
    case "Loading":
    break;
    
    case "Playable":
      this.play();
      this._buffering = false;
    break;    
    
    case "Complete":
      this.LOG("Complete.", "CoreQT");
      this.play();
      this._buffering = false;
      clearInterval(this._waitForPlayInterval);
      this._waitForPlayInterval = 0;
      this._waitRetryCount = 0;
    break;
    
    default:
      if(playerStatus == null || playerStatus.indexOf("Error") != -1)
      {
        this.cleanupOnError();
      }
  }
  
  return;
};

CoreQT.prototype.pokingUrlListener = function ( )
{
  if(gQTCore._request.readyState == 2)
  {    
    if(gQTCore._request.status == 200 &&
       gQTCore._request.channel.contentLength > 4096 )
    {
      gQTCore._availability = gQTCore.REMOTE_FILE_AVAILABLE;
      gQTCore.playURL(gQTCore._url);
    }
    else
	{
      gQTCore._availability = gQTCore.FILE_NOT_AVAILABLE;
      gQTCore._buffering = false;
      gQTCore._playing = false;
	}
    gQTCore._pokingUrl = false;
    gQTCore._request.abort();
  }
}

CoreQT.prototype.verifyFileAvailability = function ( aURL )
{
  if(!aURL)
    throw Components.results.NS_ERROR_INVALID_ARG;
  
  var localFile = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);
  var localFileURI = (Components.classes["@mozilla.org/network/simple-uri;1"]).createInstance(Components.interfaces.nsIURI);
  var localFilePath = "";
  
  try {
    localFileURI.spec = aURL;
  } catch(e) {}
  
  if(localFileURI.scheme != "file")
  {
    this._pokingUrl = true;
    
    this._request = new XMLHttpRequest();
    this._request.onreadystatechange = gQTCore.pokingUrlListener;
    this._request.open("GET", aURL, true);
    this._request.send(null);
    
    return this.NOT_LOCAL_FILE;
  }

  localFilePath = decodeURI(localFileURI.path.slice(2));
  
  var platform;
  try {
    var sysInfo =
      Components.classes["@mozilla.org/system-info;1"]
                .getService(Components.interfaces.nsIPropertyBag2);
    platform = sysInfo.getProperty("name");                                          
  }
  catch (e) {
    dump("System-info not available, trying the user agent string.\n");
    var user_agent = navigator.userAgent;
    if (user_agent.indexOf("Windows") != -1)
      platform = "Windows_NT";
    else if (user_agent.indexOf("Mac OS X") != -1)
      platform = "Darwin";
  }

  if (platform == "Windows_NT") {
    localFilePath = localFilePath.slice(1);
    localFilePath = localFilePath.replace(/\//g, '\\');
  }
  else if (platform == "Darwin") {
    localFilePath = decodeURIComponent(localFilePath);
  }

  try {
    localFile.initWithPath(localFilePath);
  } catch(e) {
    this.LOG("Bad local file." + e , "CoreQT");
    return this.FILE_NOT_AVAILABLE;
  }
  
  if(localFile.isReadable())
  {
    return this.LOCAL_FILE_AVAILABLE;  
  }

  return this.FILE_NOT_AVAILABLE;
};

// Set the url and tell it to just play it.  Eventually this talks to the media library object to make a temp playlist.
CoreQT.prototype.playURL = function ( aURL )
{
  if (this._buffering &&
	  this._pokingUrl == false)
    this.cleanupOnError();

  this._verifyObject();

  if (!aURL)
    throw Components.results.NS_ERROR_INVALID_ARG;

  var isAvailable = this.FILE_NOT_AVAILABLE;
  
  if(this._availability == this.REMOTE_FILE_AVAILABLE)
    isAvailable = this._availability;
  else
    isAvailable = this.verifyFileAvailability(aURL);

  this._url = this.sanitizeURL(aURL);
  this._paused = false;
  this._playing = true;
  this._buffering = true;  
  
  if(isAvailable == this.FILE_NOT_AVAILABLE)
    throw new Error("Local file not available");

  if(isAvailable == this.REMOTE_FILE_AVAILABLE ||
     isAvailable == this.NOT_LOCAL_FILE)
  {
    this._isremote = true;

    if(isAvailable == this.NOT_LOCAL_FILE)
      return true;
  }
  else
  {
     this._isremote = false;
  }

  this._availability = 0;
  this._object.SetURL(this._url);
  this.setVolume(this._lastVolume);
  
  if(this._waitForPlayInterval)
  {
    clearInterval(this._waitForPlayInterval);
    this._waitForPlayInterval = 0;
  }
  
  this._waitForPlayInterval = setInterval("gQTCore.waitForPlayer();", 333);

  return true;
};

CoreQT.prototype.play = function ()
{
  this._verifyObject();

  try {
    if (this._playFromBeginning) {
      try {
        this._object.Rewind();
      } catch(e) { }
      this._playFromBeginning = false;
    }
    this._object.Play();
  } catch(e) {
    this.LOG(e, "CoreQT");
  }

  this._paused = false;
  this._playing = true;

  return true;
};
  
CoreQT.prototype.pause = function ()
{
  this._verifyObject();

  try {
    this._object.Stop();
  } catch(e) {
    this.LOG(e, "CoreQT");
  }
  
  this._paused = true;
  return this._paused;
};
  
CoreQT.prototype.stop = function ()
{
  this._verifyObject();
  this._playFromBeginning = true;
  
  try {
    if (this._playing) {
      this._object.Stop();
      
      if(this._isremote)
        this.cleanupOnError();
    }
  } catch(e) {
    this.LOG(e, "CoreQT");
    
    if(this._isremote)
      this.cleanupOnError();
  }

  this._paused = false;
  this._playing = false;

  return true;
};
  
CoreQT.prototype.getPlaying = function ()
{
  this._verifyObject();

  if(this._buffering)
    return true;

  // XXXben - getPosition and getLength both return 0 when we're dealing with
  //          a stream in QT, so don't stop if this is a stream.
  var length = this.getLength();
  var position = this.getPosition();

  if ((position > 0) && (length == position))
  {
    this.stop();
  }
  return this._playing;
};
  
CoreQT.prototype.getPaused = function ()
{
  this._verifyObject();
  return this._paused;
};
  
CoreQT.prototype.getLength = function ()
{
  this._verifyObject();
  var playLength = 0;
  
  if(this._buffering)
    return 1;
  
  try {
 
    var timeScale = this._object.GetTimeScale();
    var duration = this._object.GetDuration();
  
    playLength = (duration / timeScale * 1000);
    
    if(playLength > 3579139410)
      playLength = 0;

  } catch(e) {
    this.LOG(e, "CoreQT");
  }
 
  return playLength;
};
  
CoreQT.prototype.getPosition = function ()
{
  this._verifyObject();
  var curPos = 0;
  
  if(this._buffering)
    return 0;

  try {
    var currentPos = this._object.GetTime();
    var timeScale = this._object.GetTimeScale();
    curPos = (currentPos / timeScale * 1000);
    
    if(!currentPos && this._isremote) {
      var now = new Date();
      return now.getTime() - this._starttime.getTime();
    }
  } catch(e) {
    this.LOG(e, "CoreQT");
  }
  
  return curPos;
};
  
CoreQT.prototype.setPosition = function ( pos )
{
  this._verifyObject();
  
  try {
    var timeScale = this._object.GetTimeScale();
    this._object.SetTime( pos / 1000 * timeScale );
  } catch(e) {
    this.LOG(e, "CoreQT");
  }
};
  
CoreQT.prototype.getVolume = function ()
{
  this._verifyObject();
  var curVol = this._lastVolume;
  
  try {
    curVol = this._object.GetVolume();
  } catch(e) {
    this.LOG(e, "CoreQT");
  }
  
  return curVol;
};
  
CoreQT.prototype.setVolume = function ( vol )
{
  this._verifyObject();
  this._lastVolume = vol;
  
  try {
    this._object.SetVolume(vol);
  } catch(e) {
    this.LOG(e, "CoreQT");
  }
};
  
CoreQT.prototype.getMute = function ()
{
  this._verifyObject();
  return this._muted;
};
  
CoreQT.prototype.setMute = function ( mute )
{
  this._verifyObject();
  this._object.SetMute(mute);
  this._muted = mute;
};

CoreQT.prototype.getMetadata = function ( key )
{
  this._verifyObject();
  return "";
};

CoreQT.prototype.goFullscreen = function ()
{
  this._verifyObject();
  // QT has no fullscreen functionality exposed to its plugin :(
  return true;
};
  
CoreQT.prototype.isMediaURL = function(aURL) {
  if( ( aURL.indexOf ) && 
      (
        // Protocols at the beginning
        ( aURL.indexOf( "rtsp:" ) == 0 ) ||
        // File extensions at the end
        ( aURL.indexOf( ".mp3" ) == ( aURL.length - 4 ) ) ||
        ( aURL.indexOf( ".m4a" ) == ( aURL.length - 4 ) ) ||
        ( aURL.indexOf( ".mov" ) == ( aURL.length - 4 ) ) ||
        ( aURL.indexOf( ".mpg" ) == ( aURL.length - 4 ) ) ||
        ( aURL.indexOf( ".mp4" ) == ( aURL.length - 4 ) )
      )
    )
  {
    return true;
  }
  return false;
}

CoreQT.prototype.isVideoURL = function ( aURL )
{
  if ( ( aURL.indexOf ) && 
        (
          ( aURL.indexOf( ".mov" ) == ( aURL.length - 4 ) ) ||
          ( aURL.indexOf( ".mpg" ) == ( aURL.length - 4 ) ) ||
          ( aURL.indexOf( ".mp4" ) == ( aURL.length - 4 ) )
        )
      )
  {
    return true;
  }
  return false;
}

/**
  * See nsISupports.idl
  */
CoreQT.prototype.QueryInterface = function(iid) 
{
  if (!iid.equals(Components.interfaces.sbICoreWrapper) &&
      !iid.equals(Components.interfaces.nsISupports))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  return this;
};

/**
 * ----------------------------------------------------------------------------
 * Global variables and autoinitialization.
 * ----------------------------------------------------------------------------
 */
try {
  var gQTCore = new CoreQT();
}
catch (err) {
  dump("ERROR!!! coreQT failed to create properly.");
}

function SetQTObject()
{
  var theDocumentQTInstance = document.getElementById( coreQTDocumentElementIdentifier );
  var theQTInstance = theDocumentQTInstance.contentWindow;
      
  gQTCore.setId("QT1");
  gQTCore.setObject(theQTInstance);
  gQTCore._sourcewindow = theDocumentQTInstance.contentWindow;
};

/**
  * This is the function called from a document onload handler to bind everything as playback.
  * The <html:object>s won't have their scriptable APIs attached until the onload.
  */
var coreQTDocumentElementIdentifier = "";
function CoreQTDocumentInit( id )
{
  try
  {
    var gPPS = Components.classes["@songbirdnest.com/Songbird/PlaylistPlayback;1"]
                         .getService(Components.interfaces.sbIPlaylistPlayback);
    coreQTDocumentElementIdentifier = id;
    SetQTObject();    
    gPPS.addCore(gQTCore, true);
 }
  catch ( err )
  {
    dump( "\n!!! coreQT failed to bind properly\n" + err );
  }
};
