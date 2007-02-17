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

#include "sbLocalDatabaseGUIDArray.h"

#include <nsCOMPtr.h>
#include <nsStringGlue.h>
#include <nsComponentManagerUtils.h>
#include <nsIStringEnumerator.h>
#include <sbISQLBuilder.h>
#include <sbSQLBuilderCID.h>
#include <sbIDatabaseQuery.h>
#include <DatabaseQuery.h>
#include <sbIDatabaseResult.h>
#include <sbIPropertiesManager.h>

#define DEFAULT_FETCH_SIZE 20

#define COUNT_COLUMN NS_LITERAL_STRING("count(1)")
#define GUID_COLUMN NS_LITERAL_STRING("guid")
#define OBJSORTABLE_COLUMN NS_LITERAL_STRING("obj_sortable")
#define MEDIAITEMID_COLUMN NS_LITERAL_STRING("media_item_id")
#define PROPERTIES_TABLE NS_LITERAL_STRING("resource_properties")
#define MEDIAITEMS_TABLE NS_LITERAL_STRING("media_items")

NS_IMPL_ISUPPORTS1(sbLocalDatabaseGUIDArray, sbILocalDatabaseGUIDArray)

sbLocalDatabaseGUIDArray::sbLocalDatabaseGUIDArray() :
  mBaseConstraintValue(0),
  mFetchSize(DEFAULT_FETCH_SIZE),
  mAsync(PR_FALSE),
  mLength(0),
  mValid(PR_FALSE)
{
}

sbLocalDatabaseGUIDArray::~sbLocalDatabaseGUIDArray()
{
  Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::GetDatabaseGUID(nsAString& aDatabaseGUID)
{
  aDatabaseGUID = mDatabaseGUID;

  return NS_OK;
}
NS_IMETHODIMP sbLocalDatabaseGUIDArray::SetDatabaseGUID(const nsAString& aDatabaseGUID)
{
  mDatabaseGUID = aDatabaseGUID;

  return Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::GetBaseTable(nsAString& aBaseTable)
{
  aBaseTable = mBaseTable;

  return NS_OK;
}
NS_IMETHODIMP sbLocalDatabaseGUIDArray::SetBaseTable(const nsAString& aBaseTable)
{
  mBaseTable = aBaseTable;

  return Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::GetBaseConstraintColumn(nsAString& aBaseConstraintColumn)
{
  aBaseConstraintColumn = mBaseConstraintColumn;

  return NS_OK;
}
NS_IMETHODIMP sbLocalDatabaseGUIDArray::SetBaseConstraintColumn(const nsAString& aBaseConstraintColumn)
{
  mBaseConstraintColumn = aBaseConstraintColumn;

  return Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::GetBaseConstraintValue(PRUint32 *aBaseConstraintValue)
{
  *aBaseConstraintValue = mBaseConstraintValue;

  return NS_OK;
}
NS_IMETHODIMP sbLocalDatabaseGUIDArray::SetBaseConstraintValue(PRUint32 aBaseConstraintValue)
{
  mBaseConstraintValue = aBaseConstraintValue;

  return Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::GetFetchSize(PRUint32 *aFetchSize)
{
  *aFetchSize = mFetchSize;

  return NS_OK;
}
NS_IMETHODIMP sbLocalDatabaseGUIDArray::SetFetchSize(PRUint32 aFetchSize)
{
  NS_ENSURE_ARG_MIN(aFetchSize, 1);
  mFetchSize = aFetchSize;

  return NS_OK;
}

/* attribute boolean isAsync; */
NS_IMETHODIMP sbLocalDatabaseGUIDArray::GetIsAsync(PRBool *aIsAsync)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP sbLocalDatabaseGUIDArray::SetIsAsync(PRBool aIsAsync)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute unsigned long length; */
NS_IMETHODIMP sbLocalDatabaseGUIDArray::GetLength(PRUint32 *aLength)
{
  nsresult rv;

  if (mValid == PR_FALSE) {
    rv = Initalize();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aLength = mLength;
  return NS_OK;
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::AddSort(const nsAString& aProperty,
                                                PRBool aAscending)
{
  // TODO: Check for valid properties
  SortSpec* ss = mSorts.AppendElement();
  NS_ENSURE_TRUE(ss, NS_ERROR_OUT_OF_MEMORY);

  ss->property  = aProperty;
  ss->ascending = aAscending;

  return Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::ClearSorts()
{
  mSorts.Clear();

  return Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::AddFilter(const nsAString& aProperty,
                                                  nsIStringEnumerator *aValues,
                                                  PRBool aIsSearch)
{
  // TODO: Check for valid properties
  NS_ENSURE_ARG_POINTER(aValues);

  nsresult rv;

  FilterSpec* fs = mFilters.AppendElement();
  NS_ENSURE_TRUE(fs, NS_ERROR_OUT_OF_MEMORY);

  fs->property = aProperty;
  fs->isSearch = aIsSearch;

  // Copy the values from the enumerator into an array
  PRBool hasMore;
  rv = aValues->HasMore(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  while (hasMore) {
    nsAutoString s;
    rv = aValues->GetNext(s);
    NS_ENSURE_SUCCESS(rv, rv);
    fs->values.AppendElement(s);
    rv = aValues->HasMore(&hasMore);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::ClearFilters()
{
  mFilters.Clear();

  return Invalidate();
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::GetByIndex(PRUint32 aIndex,
                                                   nsAString& _retval)
{
  nsresult rv;

  if (mValid == PR_FALSE) {
    rv = Initalize();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  //printf("Get index %d, array length %d, cache length %d\n", aIndex, mLength, mCache.Length());

  if (aIndex >= mLength) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Check to see if we have this index in cache
  if (aIndex < mCache.Length()) {
    nsAString* guid = mCache[aIndex];
    if (guid) {
      _retval.Assign(*guid);
      return NS_OK;
    }
  }

  // Cache miss
  rv = FetchRows(aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAString* guid = mCache[aIndex];
  _retval.Assign(*guid);

  return NS_OK;
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::Invalidate()
{
  if (mValid == PR_FALSE) {
    return NS_OK;
  }

  PRUint32 len = mCache.Length();
  for (PRUint32 i = 0; i < len; i++) {
    const nsString* str = mCache[i];
    if (str) {
      delete str;
    }
  }
  mCache.Clear();

  if(mPrimarySortKeyPositionCache.IsInitialized()) {
    mPrimarySortKeyPositionCache.Clear();
  }

  mValid = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP sbLocalDatabaseGUIDArray::Initalize()
{
  nsresult rv;

  // Make sure we have a database and a base table
  if (mDatabaseGUID.IsEmpty() || mBaseTable.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // Make sure we have at least one sort
  if (mSorts.Length() == 0) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mValid == PR_TRUE) {
    rv = Invalidate();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = UpdateQueries();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = UpdateLength();
  NS_ENSURE_SUCCESS(rv, rv);

  /*
   * Determine where to put null values based on the null sort policy of the
   * primary sort property and how it is being sorted
   */
  PRUint32 nullSort;
  rv = GetPropertyNullSort(mSorts[0].property, &nullSort);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (nullSort) {
    case sbIProperty::SORT_NULL_SMALL:
      mNullsFirst = mSorts[0].ascending;
    break;
    case sbIProperty::SORT_NULL_BIG:
      mNullsFirst = !mSorts[0].ascending;
    break;
    case sbIProperty::SORT_NULL_FIRST:
      mNullsFirst = PR_TRUE;
    break;
    case sbIProperty::SORT_NULL_LAST:
      mNullsFirst = PR_FALSE;
    break;
  }

  if (mNullsFirst) {
    mQueryX  = mNullQuery;
    mQueryY  = mPrimarySortQuery;
    mLengthX = mLength - mNonNullLength;
  }
  else {
    mQueryX  = mPrimarySortQuery;
    mQueryY  = mNullQuery;
    mLengthX = mNonNullLength;
  }

  mValid = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::UpdateLength()
{
  nsresult rv;

  rv = RunLengthQuery(mFullLengthQuery, &mLength);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mNotNullLengthQuery.IsEmpty()) {
    rv = RunLengthQuery(mNotNullLengthQuery, &mNonNullLength);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    mNonNullLength = mLength;
  }

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::RunLengthQuery(const nsAString& aSql,
                                         PRUint32* _retval)
{
  nsresult rv;
  PRInt32 dbOk;

  nsCOMPtr<sbIDatabaseQuery> query;
  rv = MakeQuery(aSql, getter_AddRefs(query));
  NS_ENSURE_SUCCESS(rv, rv);

  // Execute the length query
  rv = query->Execute(&dbOk);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(dbOk, dbOk);

  rv = query->WaitForCompletion(&dbOk);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(dbOk, dbOk);

  nsCOMPtr<sbIDatabaseResult> result;
  rv = query->GetResultObject(getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount;
  rv = result->GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure we get one row back
  NS_ENSURE_TRUE(rowCount == 1, NS_ERROR_UNEXPECTED);

  nsAutoString countStr;
  rv = result->GetRowCell(0, 0, countStr);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = countStr.ToInteger(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::UpdateQueries()
{

  /*
   * Generate a SQL statement that applies the current filter, search, and
   * primary sort for the supplied base table and constraints.
   */
  nsresult rv;
  nsAutoString sql;

  PRBool isFullLibrary = mBaseTable.Equals(MEDIAITEMS_TABLE);

  nsCOMPtr<sbISQLSelectBuilder> builder =
    do_CreateInstance(SB_SQLBUILDER_SELECT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  /*
   * There are four classes of queries we need to create here:
   *
   * 1. Not filtered, full library
   * 2. Not filtered, partial library
   * 3. Filtered, full library
   * 4. Filtered, partial library
   *
   * For each class, two queries are created -- one to return the row count
   * of the query result and one to return a result of a list of guids sorted
   * by the primary sort key within a range/offset.
   *
   * Futhermore, there is a case where additional queries must be created.
   * When you have a not filtered query where the primary sort property is in
   * the resource_properties table, we must create additional queries to count
   * and return the guids for the rows where the primary sort key value is
   * null.
   *
   * Also, note that small changes have to be made in the query depending if
   * the filter or sort property lives in the main table or in the properties
   * table.
   *
   * Hey, I know this is pretty complicated, but what do you expect if you
   * want all this complex functionality exposed as a simple array??
   */
  if (mFilters.IsEmpty()) {
    if (isFullLibrary) {
      /*
       * 1. Not filtered, full library
       *
       * Create the full count query for the full library
       */
      rv = builder->SetBaseTableName(mBaseTable);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->SetBaseTableAlias(NS_LITERAL_STRING("_base"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddColumn(EmptyString(), COUNT_COLUMN);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->ToString(mFullLengthQuery);
      NS_ENSURE_SUCCESS(rv, rv);

      /*
       * Create the sort query Add the sort to this query and make it return
       * guids
       */
      rv = builder->ClearColumns();
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddColumn(NS_LITERAL_STRING("_base"),
                              NS_LITERAL_STRING("guid"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = AddPrimarySortToQuery(builder, NS_LITERAL_STRING("_base"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->SetOffsetIsParameter(PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->SetLimitIsParameter(PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->ToString(mPrimarySortQuery);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->Reset();
      NS_ENSURE_SUCCESS(rv, rv);

      /*
       * Create the non-null count query for the full library only if it
       * might be different than the full count.  It might be different when
       * the primary sort is on a non top level property.
       */
      if (!IsTopLevelProperty(mSorts[0].property)) {
        rv = builder->SetBaseTableName(PROPERTIES_TABLE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->AddColumn(EmptyString(), COUNT_COLUMN);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<sbISQLBuilderCriterion> criterion;
        rv = builder->CreateMatchCriterionLong(EmptyString(),
                                               NS_LITERAL_STRING("property_id"),
                                               sbISQLSelectBuilder::MATCH_EQUALS,
                                               GetPropertyId(mSorts[0].property),
                                               getter_AddRefs(criterion));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = builder->AddCriterion(criterion);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->ToString(mNotNullLengthQuery);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->Reset();
        NS_ENSURE_SUCCESS(rv, rv);

        /*
         * Create the guid range query for rows where the primary sort key
         * value is null
         */
        rv = builder->SetBaseTableName(MEDIAITEMS_TABLE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->SetBaseTableAlias(NS_LITERAL_STRING("_base"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->AddColumn(NS_LITERAL_STRING("_base"), GUID_COLUMN);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->AddColumn(EmptyString(), NS_LITERAL_STRING("''"));
        NS_ENSURE_SUCCESS(rv, rv);

        /*
         * Left join the properties table to the media items table including
         * a constraint on the property we are sorting on
         */
        nsCOMPtr<sbISQLBuilderCriterion> criterionGuid;
        rv = builder->CreateMatchCriterionTable(NS_LITERAL_STRING("_p0"),
                                                GUID_COLUMN,
                                                sbISQLSelectBuilder::MATCH_EQUALS,
                                                NS_LITERAL_STRING("_base"),
                                                GUID_COLUMN,
                                                getter_AddRefs(criterionGuid));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<sbISQLBuilderCriterion> criterionProperty;
        rv = builder->CreateMatchCriterionLong(NS_LITERAL_STRING("_p0"),
                                               NS_LITERAL_STRING("property_id"),
                                               sbISQLSelectBuilder::MATCH_EQUALS,
                                               GetPropertyId(mSorts[0].property),
                                               getter_AddRefs(criterionProperty));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->CreateAndCriterion(criterionGuid,
                                         criterionProperty,
                                         getter_AddRefs(criterion));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->AddJoinWithCriterion(sbISQLSelectBuilder::JOIN_LEFT,
                                           PROPERTIES_TABLE,
                                           NS_LITERAL_STRING("_p0"),
                                           criterion);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->CreateMatchCriterionNull(NS_LITERAL_STRING("_p0"),
                                               OBJSORTABLE_COLUMN,
                                               sbISQLSelectBuilder::MATCH_EQUALS,
                                               getter_AddRefs(criterion));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->AddCriterion(criterion);
        NS_ENSURE_SUCCESS(rv, rv);

        /*
         * Order the results by guid so they always appear in the same order
         */
        rv = builder->AddOrder(NS_LITERAL_STRING("_p0"), GUID_COLUMN, true);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->SetOffsetIsParameter(PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->SetLimitIsParameter(PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->ToString(mNullQuery);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->Reset();
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        /*
         * Setting this to null means that the not null length is the same as
         * the total length
         */
        mNotNullLengthQuery.Truncate();
        mNullQuery.Truncate();
      }
    }
    else {
      /*
       * 2. Not filtered, partial library
       *
       * Create the full count query for the partial library
       */
      rv = builder->SetBaseTableName(mBaseTable);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->SetBaseTableAlias(NS_LITERAL_STRING("_base"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddColumn(EmptyString(), COUNT_COLUMN);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<sbISQLBuilderCriterion> criterion;
      rv = builder->CreateMatchCriterionLong(NS_LITERAL_STRING("_base"),
                                             mBaseConstraintColumn,
                                             sbISQLSelectBuilder::MATCH_EQUALS,
                                             mBaseConstraintValue,
                                             getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddCriterion(criterion);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->ToString(mFullLengthQuery);
      NS_ENSURE_SUCCESS(rv, rv);

      // TODO: Null count query
    }
  }
  else {

    /*
     * We don't need a not-null length query since there is a filer applied
     * that will always eliminate nulls
     */
    mNotNullLengthQuery.Truncate();
    mNullQuery.Truncate();

    rv = builder->AddColumn(EmptyString(), COUNT_COLUMN);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isFullLibrary) {
      /*
       * 3. Filtered, full library
       */
      rv = AddFiltersToQuery(builder, EmptyString());
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      /*
       * 4. Filtered, partial library
       *
       * If we are not dealing with the full library, we need to join the base
       * table then join the media items table so we have a guid column to join
       * the properties table to
       */
      rv = builder->SetBaseTableName(mBaseTable);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->SetBaseTableAlias(NS_LITERAL_STRING("_base"));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<sbISQLBuilderCriterion> criterion;
      rv = builder->CreateMatchCriterionLong(NS_LITERAL_STRING("_base"),
                                             mBaseConstraintColumn,
                                             sbISQLSelectBuilder::MATCH_EQUALS,
                                             mBaseConstraintValue,
                                             getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddCriterion(criterion);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddJoin(sbISQLSelectBuilder::JOIN_INNER,
                            MEDIAITEMS_TABLE,
                            NS_LITERAL_STRING("_mi"),
                            MEDIAITEMID_COLUMN,
                            NS_LITERAL_STRING("_base"),
                            NS_LITERAL_STRING("member_media_item_id"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = AddFiltersToQuery(builder, NS_LITERAL_STRING("_mi"));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = builder->ToString(mFullLengthQuery);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = builder->Reset();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  /*
   * Generate the resort query, if needed
   */
  PRUint32 numSorts = mSorts.Length();
  if (numSorts > 1) {
    nsCOMPtr<sbISQLBuilderCriterion> criterion;

    rv = builder->AddColumn(NS_LITERAL_STRING("_base"), GUID_COLUMN);
    NS_ENSURE_SUCCESS(rv, rv);

    if (IsTopLevelProperty(mSorts[0].property)) {
      rv = builder->SetBaseTableName(MEDIAITEMS_TABLE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      rv = builder->SetBaseTableName(PROPERTIES_TABLE);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = builder->CreateMatchCriterionLong(NS_LITERAL_STRING("_base"),
                                             NS_LITERAL_STRING("property_id"),
                                             sbISQLSelectBuilder::MATCH_EQUALS,
                                             GetPropertyId(mSorts[0].property),
                                             getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddCriterion(criterion);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = builder->SetBaseTableAlias(NS_LITERAL_STRING("_base"));
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i = 1; i < numSorts; i++) {
      nsAutoString joinedAlias(NS_LITERAL_STRING("_p"));
      joinedAlias.AppendInt(i);

      nsCOMPtr<sbISQLBuilderCriterion> criterionGuid;
      rv = builder->CreateMatchCriterionTable(joinedAlias,
                                              GUID_COLUMN,
                                              sbISQLSelectBuilder::MATCH_EQUALS,
                                              NS_LITERAL_STRING("_base"),
                                              GUID_COLUMN,
                                              getter_AddRefs(criterionGuid));
      NS_ENSURE_SUCCESS(rv, rv);

      if (IsTopLevelProperty(mSorts[i].property)) {
        rv = builder->AddJoinWithCriterion(sbISQLSelectBuilder::JOIN_INNER,
                                           MEDIAITEMS_TABLE,
                                           joinedAlias,
                                           criterion);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString columnName;
        rv = GetTopLevelPropertyColumn(mSorts[i].property, columnName);
        NS_ENSURE_SUCCESS(rv, rv);

        builder->AddOrder(joinedAlias,
                          columnName,
                          mSorts[i].ascending);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        nsCOMPtr<sbISQLBuilderCriterion> criterionProperty;
        rv = builder->CreateMatchCriterionLong(joinedAlias,
                                               NS_LITERAL_STRING("property_id"),
                                               sbISQLSelectBuilder::MATCH_EQUALS,
                                               GetPropertyId(mSorts[i].property),
                                               getter_AddRefs(criterionProperty));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->CreateAndCriterion(criterionGuid,
                                         criterionProperty,
                                         getter_AddRefs(criterion));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->AddJoinWithCriterion(sbISQLSelectBuilder::JOIN_LEFT,
                                           PROPERTIES_TABLE,
                                           joinedAlias,
                                           criterion);
        NS_ENSURE_SUCCESS(rv, rv);

        builder->AddOrder(joinedAlias,
                          OBJSORTABLE_COLUMN,
                          mSorts[i].ascending);
        NS_ENSURE_SUCCESS(rv, rv);
      }

    }

    rv = builder->CreateMatchCriterionParameter(NS_LITERAL_STRING("_base"),
                                                OBJSORTABLE_COLUMN,
                                                sbISQLSelectBuilder::MATCH_EQUALS,
                                                getter_AddRefs(criterion));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = builder->AddCriterion(criterion);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = builder->ToString(mResortQuery);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = builder->Reset();
    NS_ENSURE_SUCCESS(rv, rv);

    /*
     * Generate the null resort query
     */
    rv = builder->AddColumn(NS_LITERAL_STRING("_base"), GUID_COLUMN);
    NS_ENSURE_SUCCESS(rv, rv);

    if (IsTopLevelProperty(mSorts[0].property)) {
      // TODO
    }
    else {
      rv = builder->SetBaseTableName(MEDIAITEMS_TABLE);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->SetBaseTableAlias(NS_LITERAL_STRING("_base"));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<sbISQLBuilderCriterion> criterionGuid;
      rv = builder->CreateMatchCriterionTable(NS_LITERAL_STRING("_p0"),
                                              GUID_COLUMN,
                                              sbISQLSelectBuilder::MATCH_EQUALS,
                                              NS_LITERAL_STRING("_base"),
                                              GUID_COLUMN,
                                              getter_AddRefs(criterionGuid));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<sbISQLBuilderCriterion> criterionProperty;
      rv = builder->CreateMatchCriterionLong(NS_LITERAL_STRING("_p0"),
                                             NS_LITERAL_STRING("property_id"),
                                             sbISQLSelectBuilder::MATCH_EQUALS,
                                             GetPropertyId(mSorts[0].property),
                                             getter_AddRefs(criterionProperty));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->CreateAndCriterion(criterionGuid,
                                       criterionProperty,
                                       getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddJoinWithCriterion(sbISQLSelectBuilder::JOIN_LEFT,
                                         PROPERTIES_TABLE,
                                         NS_LITERAL_STRING("_p0"),
                                         criterion);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->CreateMatchCriterionNull(NS_LITERAL_STRING("_p0"),
                                             OBJSORTABLE_COLUMN,
                                             sbISQLSelectBuilder::MATCH_EQUALS,
                                             getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddCriterion(criterion);
      NS_ENSURE_SUCCESS(rv, rv);

      for (PRUint32 i = 1; i < numSorts; i++) {
        nsAutoString joinedAlias(NS_LITERAL_STRING("_p"));
        joinedAlias.AppendInt(i);

        nsCOMPtr<sbISQLBuilderCriterion> criterionGuid;
        rv = builder->CreateMatchCriterionTable(joinedAlias,
                                                GUID_COLUMN,
                                                sbISQLSelectBuilder::MATCH_EQUALS,
                                                NS_LITERAL_STRING("_base"),
                                                GUID_COLUMN,
                                                getter_AddRefs(criterionGuid));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<sbISQLBuilderCriterion> criterionProperty;
        rv = builder->CreateMatchCriterionLong(joinedAlias,
                                               NS_LITERAL_STRING("property_id"),
                                               sbISQLSelectBuilder::MATCH_EQUALS,
                                               GetPropertyId(mSorts[i].property),
                                               getter_AddRefs(criterionProperty));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->CreateAndCriterion(criterionGuid,
                                         criterionProperty,
                                         getter_AddRefs(criterion));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = builder->AddJoinWithCriterion(sbISQLSelectBuilder::JOIN_LEFT,
                                           PROPERTIES_TABLE,
                                           joinedAlias,
                                           criterion);
        NS_ENSURE_SUCCESS(rv, rv);

        builder->AddOrder(joinedAlias,
                          OBJSORTABLE_COLUMN,
                          mSorts[i].ascending);
        NS_ENSURE_SUCCESS(rv, rv);

      }

    }

    rv = builder->ToString(mNullResortQuery);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = builder->Reset();
    NS_ENSURE_SUCCESS(rv, rv);

    /*
     * Generate the primary sort key position query
     */
    rv = builder->AddColumn(EmptyString(), COUNT_COLUMN);
    NS_ENSURE_SUCCESS(rv, rv);

    if (IsTopLevelProperty(mSorts[0].property)) {
      rv = builder->SetBaseTableName(MEDIAITEMS_TABLE);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString columnName;
      rv = GetTopLevelPropertyColumn(mSorts[0].property, columnName);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->CreateMatchCriterionParameter(EmptyString(),
                                                  columnName,
                                                  sbISQLSelectBuilder::MATCH_LESS,
                                                  getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddCriterion(criterion);
      NS_ENSURE_SUCCESS(rv, rv);

      builder->AddOrder(EmptyString(),
                        columnName,
                        mSorts[0].ascending);
      NS_ENSURE_SUCCESS(rv, rv);

    }
    else {
      rv = builder->SetBaseTableName(PROPERTIES_TABLE);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = builder->CreateMatchCriterionLong(EmptyString(),
                                             NS_LITERAL_STRING("property_id"),
                                             sbISQLSelectBuilder::MATCH_EQUALS,
                                             GetPropertyId(mSorts[0].property),
                                             getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddCriterion(criterion);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->CreateMatchCriterionParameter(EmptyString(),
                                                  OBJSORTABLE_COLUMN,
                                                  sbISQLSelectBuilder::MATCH_LESS,
                                                  getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = builder->AddCriterion(criterion);
      NS_ENSURE_SUCCESS(rv, rv);

      builder->AddOrder(EmptyString(),
                        OBJSORTABLE_COLUMN,
                        mSorts[0].ascending);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = builder->ToString(mPrimarySortKeyPositionQuery);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = builder->Reset();
    NS_ENSURE_SUCCESS(rv, rv);

  }

 return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::MakeQuery(const nsAString& aSql,
                                    sbIDatabaseQuery** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  printf("**************** make query: %s **************\n", NS_ConvertUTF16toUTF8(aSql).get());

  nsresult rv;

  nsCOMPtr<sbIDatabaseQuery> query =
    do_CreateInstance(SONGBIRD_DATABASEQUERY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->SetDatabaseGUID(mDatabaseGUID);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->SetAsyncQuery(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->AddQuery(aSql);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = query);
  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::AddFiltersToQuery(sbISQLSelectBuilder *aBuilder,
                                            const nsAString& baseAlias)
{
  nsresult rv;

  nsAutoString baseTableAlias(baseAlias);

  PRUint32 len = mFilters.Length();
  for (PRUint32 i = 0; i < len; i++) {
    const FilterSpec& fs = mFilters[i];

    nsAutoString tableAlias;
    tableAlias.AppendLiteral("_p");
    tableAlias.AppendInt(i);

    PRBool isTopLevelProperty = IsTopLevelProperty(fs.property);

    nsAutoString joinedTableName;
    // If the filtered property is a top level property, join the top level
    // table
    if (isTopLevelProperty) {
      joinedTableName = MEDIAITEMS_TABLE;
    }
    else {
      joinedTableName = PROPERTIES_TABLE;
    }

    if (i == 0) {

      // If the base table alias of the query is empty we should use the first
      // filter as the base table.  Otherwise join the first filter to the base
      // table
      if (baseTableAlias.IsEmpty()) {
        rv = aBuilder->SetBaseTableName(joinedTableName);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = aBuilder->SetBaseTableAlias(tableAlias);
        NS_ENSURE_SUCCESS(rv, rv);

        baseTableAlias = tableAlias;
      }
      else {
        rv = aBuilder->AddJoin(sbISQLSelectBuilder::JOIN_INNER,
                               joinedTableName,
                               tableAlias,
                               GUID_COLUMN,
                               baseTableAlias,
                               GUID_COLUMN);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else {
      rv = aBuilder->AddJoin(sbISQLSelectBuilder::JOIN_INNER,
                             joinedTableName,
                             tableAlias,
                             GUID_COLUMN,
                             baseTableAlias,
                             GUID_COLUMN);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Add the critera
    nsCOMPtr<sbISQLBuilderCriterion> criterion;

    if (fs.isSearch) {

      // XXX: Lets not support search on top level properties
      if (isTopLevelProperty) {
        return NS_ERROR_INVALID_ARG;
      }

      // If a property is specified, add it here.  If this is empty, no
      // property contraint is added for this join which makes it search all
      // properties
      if(!fs.property.IsEmpty()) {
        rv = aBuilder->CreateMatchCriterionLong(tableAlias,
                                                NS_LITERAL_STRING("property_id"),
                                                sbISQLSelectBuilder::MATCH_EQUALS,
                                                GetPropertyId(fs.property),
                                                getter_AddRefs(criterion));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aBuilder->AddCriterion(criterion);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Add the search term
      nsAutoString searchTerm;
      searchTerm.AppendLiteral("%");
      searchTerm.Append(fs.values[0]);
      searchTerm.AppendLiteral("%");

      rv = aBuilder->CreateMatchCriterionString(tableAlias,
                                                OBJSORTABLE_COLUMN,
                                                sbISQLSelectBuilder::MATCH_LIKE,
                                                searchTerm,
                                                getter_AddRefs(criterion));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aBuilder->AddCriterion(criterion);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {

      nsCOMPtr<sbISQLBuilderCriterionIn> inCriterion;
      if (isTopLevelProperty) {

        // Add the constraint for the top level table.
        nsAutoString columnName;
        rv = GetTopLevelPropertyColumn(fs.property, columnName);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aBuilder->CreateMatchCriterionIn(tableAlias,
                                              columnName,
                                              getter_AddRefs(inCriterion));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        // Add the propery constraint for the filter
        rv = aBuilder->CreateMatchCriterionLong(tableAlias,
                                                NS_LITERAL_STRING("property_id"),
                                                sbISQLSelectBuilder::MATCH_EQUALS,
                                                GetPropertyId(fs.property),
                                                getter_AddRefs(criterion));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aBuilder->AddCriterion(criterion);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aBuilder->CreateMatchCriterionIn(tableAlias,
                                              OBJSORTABLE_COLUMN,
                                              getter_AddRefs(inCriterion));
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // For each filter value, add the term to an IN constraint
      PRUint32 numValues = fs.values.Length();
      for (PRUint32 j = 0; j < numValues; j++) {
        const nsAString& filterTerm = fs.values[j];
        rv = inCriterion->AddString(filterTerm);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      rv = aBuilder->AddCriterion(inCriterion);
      NS_ENSURE_SUCCESS(rv, rv);
    }

  }

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::AddPrimarySortToQuery(sbISQLSelectBuilder *aBuilder,
                                                const nsAString& baseAlias)
{
  nsresult rv;

  const SortSpec& ss = mSorts[0];

  /*
   * If this is a top level propery, simply add the sort
   */
  if (IsTopLevelProperty(ss.property)) {
    nsAutoString columnName;
    rv = GetTopLevelPropertyColumn(ss.property, columnName);
    NS_ENSURE_SUCCESS(rv, rv);

    /*
     * Add the output column for the sorted property so we can later use
     * this for additional sorting
     */
    rv = aBuilder->AddColumn(baseAlias,
                            columnName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aBuilder->AddOrder(baseAlias,
                            columnName,
                            ss.ascending);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    /*
     * Add the output column for the sorted property so we can later use
     * this for additional sorting
     */
    rv = aBuilder->AddColumn(NS_LITERAL_STRING("_sort"),
                            OBJSORTABLE_COLUMN);
    NS_ENSURE_SUCCESS(rv, rv);

    /*
     * Join an instance of the properties table to the base table
     */
    rv = aBuilder->AddJoin(sbISQLSelectBuilder::JOIN_INNER,
                           PROPERTIES_TABLE,
                           NS_LITERAL_STRING("_sort"),
                           GUID_COLUMN,
                           baseAlias,
                           GUID_COLUMN);
    NS_ENSURE_SUCCESS(rv, rv);

    /*
     * Restrict the sort table to the sort property
     */
    nsCOMPtr<sbISQLBuilderCriterion> criterion;
    rv = aBuilder->CreateMatchCriterionLong(NS_LITERAL_STRING("_sort"),
                                            NS_LITERAL_STRING("property_id"),
                                            sbISQLSelectBuilder::MATCH_EQUALS,
                                            GetPropertyId(ss.property),
                                            getter_AddRefs(criterion));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aBuilder->AddCriterion(criterion);
    NS_ENSURE_SUCCESS(rv, rv);

    /*
     * Add a sort on the primary sort
     */
    rv = aBuilder->AddOrder(NS_LITERAL_STRING("_sort"),
                            OBJSORTABLE_COLUMN,
                            ss.ascending);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::FetchRows(PRUint32 aRequestedIndex)
{
  nsresult rv;

  /*
   * To read the full media library, two queries are used -- one for when the
   * primary sort key has values and one for when the primary sort key has
   * no values (null values).  When sorting, items where the primary sort key
   * has no values may be sorted to the beginning or to the end of the list.
   * The diagram below describes this situation:
   *
   * A                            B                                  C
   * +--------------X-------------+-----------------Y----------------+
   *                 +------------------------------+
   *                 D                              E
   *
   * Array AC (A is at index 0 and C is the maximum index of the array)
   * represents the entire media library.  Index B represents the first item
   * in the array that requires a different query as described above. Requests
   * for items in [A, B - 1] use query X, and requests for items in [B, C] use
   * query Y.
   *
   * Index range DE (inclusive) represents some sub-array within AC.  Depending
   * on the relationship between DE to index B, different strategies need to be
   * used to query the data:
   *
   * - If DE lies entirely within [A, B - 1], use query X to return the data
   * - If DE lies entirely within [B, C], use query Y to return the data
   * - If DE lies in both [A, B - 1] and [B, C], use query X to return the data
   *   in [A, B - 1] and Y to return data in[B, C]
   *
   * Note the start index of query Y must be relative to index B, not index A.
   */

  PRUint32 indexB = mLengthX;
  PRUint32 indexC = mLength - 1;

  PRUint32 indexD = aRequestedIndex;
  PRUint32 indexE = aRequestedIndex + mFetchSize - 1;
  if (indexE > indexC) {
    indexE = indexC;
  }
  PRUint32 lengthDE = indexE - indexD + 1;

  /*
   * If DE lies entirely within [A, B - 1], use query X to return the data
   */
  if (indexE < indexB) {
    rv = ReadRowRange(mQueryX,
                      indexD,
                      lengthDE,
                      indexD,
                      mNullsFirst);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    /*
     * If DE lies entirely within [B, C], use query Y to return the data
     */
    if (indexD >= indexB) {
      rv = ReadRowRange(mQueryY,
                        indexD - indexB,
                        lengthDE,
                        indexD,
                        !mNullsFirst);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      /*
       * If DE lies in both [A, B - 1] and [B, C], use query X to return the
       * data in [A, B - 1] and Y to return data in[B, C]
       */
      rv = ReadRowRange(mQueryX,
                        indexD,
                        indexB - indexD + 1,
                        indexD,
                        mNullsFirst);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = ReadRowRange(mQueryY,
                        0,
                        indexE - indexB + 1,
                        indexB,
                        !mNullsFirst);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::ReadRowRange(const nsAString& aSql,
                                       PRUint32 aStartIndex,
                                       PRUint32 aCount,
                                       PRUint32 aDestIndexOffset,
                                       PRBool isNull)
{
  nsresult rv;
  PRInt32 dbOk;

  NS_ENSURE_ARG_MIN(aStartIndex, 0);
  NS_ENSURE_ARG_MIN(aCount, 1);
  NS_ENSURE_ARG_MIN(aDestIndexOffset, 0);

  printf("ReadRowRange start %d count %d dest offset %d\n", aStartIndex, aCount, aDestIndexOffset);

  /*
   * Set up the query with limit and offset parameters and run it
   */
  nsCOMPtr<sbIDatabaseQuery> query;
  rv = MakeQuery(aSql, getter_AddRefs(query));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->BindInt32Parameter(0, aCount);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->BindInt32Parameter(1, aStartIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->Execute(&dbOk);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(dbOk, dbOk);

  rv = query->WaitForCompletion(&dbOk);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(dbOk, dbOk);

  nsCOMPtr<sbIDatabaseResult> result;
  rv = query->GetResultObject(getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount;
  rv = result->GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  /*
   * If there is a multi-level sort in effect, we need to apply the additional
   * level of sorts to this result
   */
  PRBool needsSorting = mSorts.Length() > 1;

  /*
   * Resize the cache so we can fit the new data
   */
  NS_ENSURE_TRUE(mCache.SetLength(aDestIndexOffset + aCount),
                 NS_ERROR_OUT_OF_MEMORY);

  nsAutoString lastSortedValue;
  PRInt32 firstIndex = 0;
  PRBool isFirstValue = PR_TRUE;
  PRBool isFirstSort = PR_TRUE;
  for (PRInt32 i = 0; i < rowCount; i++) {
    PRUnichar* guid;
    rv = result->GetRowCellPtr(i, 0, &guid);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString* str = new nsString(guid);
    NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

    NS_ENSURE_TRUE(mCache.ReplaceElementsAt(i + aDestIndexOffset, 1, str),
                   NS_ERROR_OUT_OF_MEMORY);

    if (needsSorting) {
      PRUnichar* value;
      rv = result->GetRowCellPtr(i, 1, &value);
      NS_ENSURE_SUCCESS(rv, rv);
      if (isFirstValue || !lastSortedValue.Equals(value)) {
        if (!isFirstValue) {
          rv = SortRows(aDestIndexOffset + firstIndex,
                        aDestIndexOffset + i - 1,
                        lastSortedValue,
                        isFirstSort,
                        PR_FALSE,
                        PR_FALSE,
                        isNull);
          NS_ENSURE_SUCCESS(rv, rv);
          isFirstSort = PR_FALSE;
        }
        lastSortedValue.Assign(value);
        firstIndex = i;
        isFirstValue = PR_FALSE;
      }
    }
  }

  if (needsSorting) {
    rv = SortRows(aDestIndexOffset + firstIndex,
                  aDestIndexOffset + rowCount - 1,
                  lastSortedValue,
                  isFirstSort,
                  PR_TRUE,
                  isFirstSort == PR_TRUE,
                  isNull);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  /*
   * If the number of rows returned is less than what was requested, this is
   * really bad.  Fill the rest in so we don't crash
   */
  if (rowCount < aCount) {
    NS_WARNING("Did not get the requested number of rows");
    for (PRInt32 i = 0; i < aCount - rowCount; i++) {
      nsString* str = new nsString(NS_LITERAL_STRING("error"));
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      NS_ENSURE_TRUE(mCache.ReplaceElementsAt(i + rowCount + aDestIndexOffset, 1, str),
                     NS_ERROR_OUT_OF_MEMORY);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::SortRows(PRUint32 aStartIndex,
                                   PRUint32 aEndIndex,
                                   const nsAString& aKey,
                                   PRBool aIsFirst,
                                   PRBool aIsLast,
                                   PRBool aIsOnly,
                                   PRBool aIsNull)
{
  nsresult rv;
  PRInt32 dbOk;

  printf("Sorting rows %d to %d on %s, isfirst %d isonly %d isnull %d\n", aStartIndex, aEndIndex, NS_ConvertUTF16toUTF8(aKey).get(), aIsFirst, aIsOnly, aIsNull);

  /*
   * If this is only one row and it is not the first, last, and only row in the
   * window, we don't need to sort it
   */
  if (!aIsFirst && !aIsLast && !aIsOnly && aStartIndex == aEndIndex) {
    return NS_OK;
  }

  nsCOMPtr<sbIDatabaseQuery> query;
  if(aIsNull) {
    rv = MakeQuery(mNullResortQuery, getter_AddRefs(query));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    rv = MakeQuery(mResortQuery, getter_AddRefs(query));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = query->BindStringParameter(0, aKey);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = query->Execute(&dbOk);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(dbOk, dbOk);

  rv = query->WaitForCompletion(&dbOk);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(dbOk, dbOk);

  nsCOMPtr<sbIDatabaseResult> result;
  rv = query->GetResultObject(getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount;
  rv = result->GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeLength = aEndIndex - aStartIndex + 1;

  /*
   * Make sure we get at least the number of rows back from the query that
   * we need to replace
   */
  if (rangeLength > rowCount) {
    return NS_ERROR_UNEXPECTED;
  }

  /*
   * Figure out the offset into the result set we should use to copy the query
   * results into the cache.  If this is the only sort being done for a window
   * (indicated when aIsOnly is true), we have no reference point to determine
   * the offset, so we must query for it.
   */
  PRUint32 offset = 0;
  if (aIsOnly) {
    /*
     * If we are resorting a null range, we can use the cached non null length
     * to calculate the offset
     */
    if (aIsNull) {
      if (mNullsFirst) {
        offset = 0;
      }
      else {
        offset = aStartIndex - mNonNullLength;
      }
    }
    else {
      PRUint32 position;
      rv = GetPrimarySortKeyPosition(aKey, &position);
      NS_ENSURE_SUCCESS(rv, rv);

      offset = aStartIndex - position;
    }
  }
  else {
    /*
     * If this range is at the top of the window, set the offset such that
     * we will copy the tail end of the result set
     */
    if (aIsFirst) {
      offset = rowCount - rangeLength;
    }
    else {
      /*
       * Otherwise just copy the entire result set into the cache
       */
      offset = 0;
    }
  }

  /*
   * Copy the rows from the query result into the cache starting at the
   * calculated offset
   */
  for (PRInt32 i = 0; i < rangeLength; i++) {
    PRUnichar* guid;
    rv = result->GetRowCellPtr(offset + i, 0, &guid);
    NS_ENSURE_SUCCESS(rv, rv);

    mCache[i + aStartIndex]->Assign(guid);
  }

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::GetPrimarySortKeyPosition(const nsAString& aValue,
                                                    PRUint32 *_retval)
{
  nsresult rv;

  /*
   * Make sure the cache is initalized
   */
  if (!mPrimarySortKeyPositionCache.IsInitialized()) {
    mPrimarySortKeyPositionCache.Init(100);
  }

  PRUint32 position;
  if (!mPrimarySortKeyPositionCache.Get(aValue, &position)) {
    PRInt32 dbOk;

    nsCOMPtr<sbIDatabaseQuery> query;
    rv = MakeQuery(mPrimarySortKeyPositionQuery, getter_AddRefs(query));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = query->BindStringParameter(0, aValue);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = query->Execute(&dbOk);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_SUCCESS(dbOk, dbOk);

    rv = query->WaitForCompletion(&dbOk);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_SUCCESS(dbOk, dbOk);

    nsCOMPtr<sbIDatabaseResult> result;
    rv = query->GetResultObject(getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 rowCount;
    rv = result->GetRowCount(&rowCount);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(rowCount == 1, NS_ERROR_UNEXPECTED);

    nsAutoString countStr;
    rv = result->GetRowCell(0, 0, countStr);
    NS_ENSURE_SUCCESS(rv, rv);

    position = countStr.ToInteger(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mPrimarySortKeyPositionCache.Put(aValue, position);
  }

  *_retval = position;
  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::GetTopLevelPropertyColumn(const nsAString& aProperty,
                                                    nsAString& columnName)
{
  nsAutoString retval;

  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#guid")) {
    retval = NS_LITERAL_STRING("guid");
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#created")) {
    retval = NS_LITERAL_STRING("created");
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#updated")) {
    retval = NS_LITERAL_STRING("updated");
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#contentUrl")) {
    retval = NS_LITERAL_STRING("content_url");
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#contentMimeType")) {
    retval = NS_LITERAL_STRING("content_mime_type");
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#contentLength")) {
    retval = NS_LITERAL_STRING("content_length");
  }

  if (retval.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  else {
    columnName = retval;
    return NS_OK;
  }
}

PRBool
sbLocalDatabaseGUIDArray::IsTopLevelProperty(const nsAString& aProperty)
{
  // XXX: This should be replaced with a call to the properties manager
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#guid") ||
      aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#created") ||
      aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#updated") ||
      aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#contentUrl") ||
      aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#contentMimeType") ||
      aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#contentLength")) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }

}

NS_IMETHODIMP
sbLocalDatabaseGUIDArray::GetPropertyNullSort(const nsAString& aProperty,
                                              PRUint32 *_retval)
{
  // XXX: This should be replaced with a call to the properties manager
  nsAutoString retval;

  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#trackName") ||
      aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#artistName") ||
      aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#albumName")) {
    *_retval = sbIProperty::SORT_NULL_BIG;
  }
  else {
    *_retval = sbIProperty::SORT_NULL_SMALL;
  }

  return NS_OK;
}

PRInt32
sbLocalDatabaseGUIDArray::GetPropertyId(const nsAString& aProperty)
{
  // XXX: This should be replaced with a call to the properties manager
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#trackName")) {
    return 1;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#albumName")) {
    return 2;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#artistName")) {
    return 3;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#duration")) {
    return 4;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#genre")) {
    return 5;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#track")) {
    return 6;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#year")) {
    return 7;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#discNumber")) {
    return 8;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#totalDiscs")) {
    return 9;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#totalTracks")) {
    return 10;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#lastPlayTime")) {
    return 11;
  }
  if (aProperty.EqualsLiteral("http://songbirdnest.com/data/1.0#playCount")) {
    return 12;
  }

  return -1;
}

