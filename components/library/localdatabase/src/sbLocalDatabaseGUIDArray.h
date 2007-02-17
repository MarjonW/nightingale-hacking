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

#ifndef __SBLOCALDATABASESMARTMEDIALIST_H__
#define __SBLOCALDATABASESMARTMEDIALIST_H__

#include "sbILocalDatabaseGUIDArray.h"

#include <nsStringGlue.h>
#include <nsTArray.h>
#include <nsCOMPtr.h>
#include <sbIDatabaseQuery.h>
#include <sbISQLBuilder.h>
#include <nsDataHashtable.h>

class sbLocalDatabaseGUIDArray : public sbILocalDatabaseGUIDArray
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_SBILOCALDATABASEGUIDARRAY

  sbLocalDatabaseGUIDArray();

private:
  ~sbLocalDatabaseGUIDArray();

  NS_IMETHOD Initalize();

  NS_IMETHOD UpdateLength();

  NS_IMETHODIMP RunLengthQuery(const nsAString& aSql,
                               PRUint32* _retval);

  NS_IMETHOD UpdateQueries();

  PRBool IsTopLevelProperty(const nsAString& aProperty);

  NS_IMETHODIMP GetPrimarySortKeyPosition(const nsAString& aValue,
                                          PRUint32 *_retval);

  NS_IMETHODIMP GetTopLevelPropertyColumn(const nsAString& aProperty,
                                          nsAString& columnName);

  NS_IMETHODIMP GetPropertyNullSort(const nsAString& aProperty,
                                    PRUint32 *_retval);

  NS_IMETHODIMP MakeQuery(const nsAString& aSql, sbIDatabaseQuery** _retval);

  NS_IMETHODIMP AddFiltersToQuery(sbISQLSelectBuilder *aBuilder,
                                  const nsAString& baseAlias);

  NS_IMETHODIMP AddPrimarySortToQuery(sbISQLSelectBuilder *aBuilder,
                                      const nsAString& baseAlias);

  NS_IMETHODIMP FetchRows(PRUint32 aRequestedIndex);

  NS_IMETHODIMP SortRows(PRUint32 aStartIndex,
                         PRUint32 aEndIndex,
                         const nsAString& aKey,
                         PRBool aIsFirst,
                         PRBool aIsLast,
                         PRBool aIsOnly,
                         PRBool isNull);

  NS_IMETHODIMP ReadRowRange(const nsAString& aSql,
                             PRUint32 aStartIndex,
                             PRUint32 aCount,
                             PRUint32 aDestIndexOffset,
                             PRBool isNull);

  PRInt32 GetPropertyId(const nsAString& aProperty);

  struct SortSpec {
    nsString property;
    PRBool ascending;
  };

  struct FilterSpec {
    nsString property;
    nsTArray<nsString> values;
    PRBool isSearch;
  };

  // Database GUID
  // XXX: This will probably change to a path?
  nsString mDatabaseGUID;

  // Query base table
  nsString mBaseTable;

  // Optional column constraint to use for base query
  nsString mBaseConstraintColumn;

  // Optional column constraint value to use for base query
  PRUint32 mBaseConstraintValue;

  // Number of rows to featch on cache miss
  PRUint32 mFetchSize;

  // Do things async
  PRBool mAsync;

  // Length of complete array
  PRUint32 mLength;

  // Length of non-null portion of the array
  PRUint32 mNonNullLength;

  // Is the cache valid
  PRBool mValid;

  // Current sort configuration
  nsTArray<SortSpec> mSorts;

  // Current filter configuration
  nsTArray<FilterSpec> mFilters;

  // Ordered array of GUIDs
  nsTArray<nsString*> mCache;

  // Cache of primary sort key positions
  nsDataHashtable<nsStringHashKey, PRUint32> mPrimarySortKeyPositionCache;

  // Query used to count full length of the array
  nsString mFullLengthQuery;

  // Query used to count full length of the array where the primary sort key
  // is null
  nsString mNotNullLengthQuery;

  // Query used to return sorted GUIDs when the primary sort key is non-null
  nsString mPrimarySortQuery;

  // Query used to return sorted GUIDs when the primary sort key is null
  nsString mNullQuery;

  // Query used to resort chunks of results
  nsString mResortQuery;

  // Query used to resort a chunk of results with a null primary sort key
  nsString mNullResortQuery;

  // Query used to find the position of a primary sort key in the library
  nsString mPrimarySortKeyPositionQuery;

  // Cached versions of some of the above variables used my the fetch
  nsString mQueryX;
  nsString mQueryY;
  PRUint32 mLengthX;

  // How nulls are sorted
  PRBool mNullsFirst;

protected:
  /* additional members */
};

#endif /* __SBLOCALDATABASESMARTMEDIALIST_H__ */

