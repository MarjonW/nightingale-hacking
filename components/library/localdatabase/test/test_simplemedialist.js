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

/**
 * \brief Test file
 */

function countItems(enumerator) {
  var count = 0;
  while (enumerator.hasMoreElements()) {
    var item = enumerator.getNext().QueryInterface(Ci.sbIMediaItem);
    assertNotEqual(item, null);
    count++;
  }
  return count;
}

function runTest () {

  var databaseGUID = "test_localdatabaselibrary";
  createDatabase(databaseGUID);

  var library = createLibrary(databaseGUID);

  var list = library.getMediaItem("7e8dcc95-7a1d-4bb3-9b14-d4906a9952cb");
  assertList(list, "data_sort_sml101_ordinal_asc.txt");

  var item = list.getItemByIndex(0);
  assertEqual(item.guid, "3E586C1A-AD99-11DB-9321-C22AB7121F49");

  // Test contains
  item = library.getMediaItem("3E586C1A-AD99-11DB-9321-C22AB7121F49");
  var contains = list.contains(item);
  assertEqual(contains, true);

  item = library.getMediaItem("3E6DD1C2-AD99-11DB-9321-C22AB7121F49");
  contains = list.contains(item);
  assertEqual(contains, false);

  var titleProperty = "http://songbirdnest.com/data/1.0#trackName";
  var albumProperty = "http://songbirdnest.com/data/1.0#albumName";
  var genreProperty = "http://songbirdnest.com/data/1.0#genre";

  // Test getItemsByProperty(s)
  var filteredListEnumerator =
    list.getItemsByPropertyValue(titleProperty, "Train of Thought");

  assertEqual(countItems(filteredListEnumerator), 1);

  filteredListEnumerator =
    list.getItemsByPropertyValue(albumProperty, "Back in Black");

  assertEqual(countItems(filteredListEnumerator), 10);

  filteredListEnumerator =
    list.getItemsByPropertyValue(genreProperty, "KJaskjjbfjJDBs");

  assertEqual(countItems(filteredListEnumerator), 0);

  var propertyArray =
    Cc["@songbirdnest.com/Songbird/Properties/PropertyArray;1"].
    createInstance(Ci.sbIPropertyArray);

  propertyArray.appendProperty(albumProperty, "Back in Black");
  filteredListEnumerator =
    list.getItemsByPropertyValues(propertyArray);

  assertEqual(countItems(filteredListEnumerator), 10);

  propertyArray.appendProperty(titleProperty, "Rock and Roll Ain't Noise Pollution");
  propertyArray.appendProperty(titleProperty, "Shake a Leg");
  filteredListEnumerator =
    list.getItemsByPropertyValues(propertyArray);

  assertEqual(countItems(filteredListEnumerator), 2);

  propertyArray.removeElementAt(1);
  filteredListEnumerator =
    list.getItemsByPropertyValues(propertyArray);

  assertEqual(countItems(filteredListEnumerator), 1);
  
  //Test getIemByIndex, indexOf, lastIndexOf.
  var mediaItem = list.getItemByIndex(8);
  assertNotEqual(mediaItem, null);
  
  var mediaItemIndex = list.indexOf(mediaItem, 0);
  assertEqual(mediaItemIndex, 8);
  
  var indexOfException;
  try {
    mediaItemIndex = list.indexOf(mediaItem, 10);
  } catch (e) {
    indexOfException = e;
  }
  assertEqual(indexOfException.result, Cr.NS_ERROR_NOT_AVAILABLE);
  
  var indexOfException2;
  try {
    mediaItemIndex = list.indexOf(mediaItem, 45);
  } catch (e) {
    indexOfException2 = e;
  }
  assertEqual(indexOfException2.result, Cr.NS_ERROR_INVALID_ARG);

  mediaItemIndex = list.lastIndexOf(mediaItem, 0);
  assertEqual(mediaItemIndex, 8);
  
  var lastIndexOfException;
  try {
    mediaItemIndex = list.lastIndexOf(mediaItem, 10);
  } catch (e) {
    lastIndexOfException = e;
  }
  assertEqual(lastIndexOfException.result, Cr.NS_ERROR_NOT_AVAILABLE);
  
  var lastIndexOfException2;
  try {
    mediaItemIndex = list.lastIndexOf(mediaItem, 45);
  } catch (e) {
    lastIndexOfException2 = e;
  }
  assertEqual(lastIndexOfException2.result, Cr.NS_ERROR_INVALID_ARG);

  //Test add, addSome, addAll
  var data = readList("data_sort_sml101_ordinal_asc.txt");

  list = library.getMediaItem("7e8dcc95-7a1d-4bb3-9b14-d4906a9952cb");
  item = library.getMediaItem("3E6DD1C2-AD99-11DB-9321-C22AB7121F49");
  var oldlength = list.length;

  list.add(item);
  data.push(item.guid);
  assertEqual(list.contains(item), true);
  assertEqual(list.length, oldlength + 1);
  assertList(list, data);

  list.add(item);
  data.push(item.guid);
  assertEqual(list.contains(item), true);
  assertEqual(list.length, oldlength + 2);
  assertList(list, data);

  list.add(list);
  data.push(list.guid);
  assertEqual(list.contains(item), true);
  assertEqual(list.length, oldlength + 3);
  assertList(list, data);

  // Add the entire database to the list
  var view = library.getMediaItem("songbird:view");
  var a = readList("data_sort_created_asc.txt");

  list.addAll(view);
  a.forEach(function(e) { data.push(e); });
  assertList(list, data);
  assertEqual(list.length, oldlength + 3 + view.length);

  // And add it again
  list.addSome(view.items);
  a.forEach(function(e) { data.push(e); });
  assertList(list, data);
  assertEqual(list.length, oldlength + 3 + view.length + view.length);

  // Test insertBefore.  These tests seem a bit random but they are testing
  // all the code paths in sbLocalDatabaseSimpleMediaList::GetBeforeOrdinal
  createDatabase(databaseGUID);
  library = createLibrary(databaseGUID);
  list = library.getMediaItem("7e8dcc95-7a1d-4bb3-9b14-d4906a9952cb");
  a = readList("data_sort_sml101_ordinal_asc.txt");

  // insert at top
  item = library.getMediaItem("3E6DD1C2-AD99-11DB-9321-C22AB7121F49");
  list.insertBefore(0, item);
  a.unshift(item.guid);
  assertList(list, a);

  // insert before last
  item = library.getMediaItem("3E6D8050-AD99-11DB-9321-C22AB7121F49");
  list.insertBefore(list.length - 1, item);
  a.splice(a.length - 1, 0, item.guid);
  assertList(list, a);

  // insert above the previous insert
  item = library.getMediaItem("3E6D3050-AD99-11DB-9321-C22AB7121F49");
  list.insertBefore(list.length - 2, item);
  a.splice(a.length - 2, 0, item.guid);
  assertList(list, a);

  // insert befoe last again
  item = library.getMediaItem("3E6CDB1E-AD99-11DB-9321-C22AB7121F49");
  list.insertBefore(list.length - 1, item);
  a.splice(a.length - 1, 0, item.guid);
  assertList(list, a);

  // insert above the previous insert
  item = library.getMediaItem("3E6C8D80-AD99-11DB-9321-C22AB7121F49");
  list.insertBefore(list.length - 2, item);
  a.splice(a.length - 2, 0, item.guid);
  assertList(list, a);

  // test bad index
  try {
    list.insertBefore(list.length, item);
    fail("NS_ERROR_ILLEGAL_VALUE not thrown");
  }
  catch(e) {
    assertEqual(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  // Test moveBefore
  createDatabase(databaseGUID);
  library = createLibrary(databaseGUID);
  list = library.getMediaItem("7e8dcc95-7a1d-4bb3-9b14-d4906a9952cb");
  a = readList("data_sort_sml101_ordinal_asc.txt");

  // move from second to first
  list.moveBefore(1, 0);
  a.unshift(a.splice(1, 1)[0]);
  assertList(list, a);

  // move from first to before last
  list.moveBefore(0, list.length - 1);
  var guid = a.splice(0, 1)[0];
  a.splice(a.length - 1, 0, guid);
  assertList(list, a);

  // Test moveLast
  list.moveLast(0);
  guid = a.splice(0, 1)[0];
  a.push(guid);
  assertList(list, a);

}

