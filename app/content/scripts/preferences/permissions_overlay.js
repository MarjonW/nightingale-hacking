/*
 //
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2007 POTI, Inc.
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

var gSongbirdPermissionsOverlay = {
  prompt: null,
  blocking: null,
  prefs: null,
  onLoad: function(event) {
    window.removeEventListener('load', gSongbirdPermissionsOverlay.onLoad, false);
    
    if (!window.arguments || !window.arguments[0] ||
        !window.arguments[0].blocking) {
      // a blocking object wasn't passed in. nothing to do
      return;
    }
    gSongbirdPermissionsOverlay.blocking = window.arguments[0].blocking;

    var permissionsText = document.getElementById('permissionsText');
    if (!permissionsText) {
      return;
    }
    
    gSongbirdPermissionsOverlay.prefs =
      Components.classes['@mozilla.org/preferences-service;1']
      .getService(Components.interfaces.nsIPrefBranch);
    
    var settings = document.createElement('description');
    settings.appendChild(document.createTextNode(gSongbirdPermissionsOverlay.blocking.settings));
    permissionsText.parentNode.insertBefore(settings, permissionsText);    
    
    gSongbirdPermissionsOverlay.prompt = document.createElement('checkbox');
    permissionsText.parentNode.insertBefore(gSongbirdPermissionsOverlay.prompt,
      permissionsText);
    gSongbirdPermissionsOverlay.prompt.label = gSongbirdPermissionsOverlay.blocking.prompt;
    try {
      gSongbirdPermissionsOverlay.prompt.checked =
        gSongbirdPermissionsOverlay.prefs.getBoolPref(
          gSongbirdPermissionsOverlay.blocking.pref);
    } catch (e) {
      // don't fail on a missing pref
    }
    gSongbirdPermissionsOverlay.prompt.addEventListener('command',
      gSongbirdPermissionsOverlay.onCommand, false);

    window.addEventListener('close', gSongbirdPermissionsOverlay.onClose, false);
  },
  onCommand: function(event) {
    gSongbirdPermissionsOverlay.prefs.setBoolPref(
        gSongbirdPermissionsOverlay.blocking.pref,
        gSongbirdPermissionsOverlay.prompt.checked);
  },
  onClose: function(event) {
    gSongbirdPermissionsOverlay.prompt.removeEventListener('command',
      gSongbirdPermissionsOverlay.onCommand, false);
    window.removeEventListener('close', gSongbirdPermissionsOverlay.onClose,
      false);
  }
};
window.addEventListener('load', gSongbirdPermissionsOverlay.onLoad, false);


