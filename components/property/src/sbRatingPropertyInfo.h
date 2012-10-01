/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2008 POTI, Inc.
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

#ifndef __SBRATINGPROPERTYINFO_H__
#define __SBRATINGPROPERTYINFO_H__

#include "sbImmutablePropertyInfo.h"

#include <sbIPropertyManager.h>
#include <sbITreeViewPropertyInfo.h>
#include <sbIClickablePropertyInfo.h>

#include <nsCOMPtr.h>
#include <nsStringGlue.h>
#include "sbPropertyInfo.h"

class sbRatingPropertyInfo : public sbImmutablePropertyInfo,
                             public sbIClickablePropertyInfo,
                             public sbITreeViewPropertyInfo
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_SBICLICKABLEPROPERTYINFO
  NS_DECL_SBITREEVIEWPROPERTYINFO

  sbRatingPropertyInfo(const nsAString& aPropertyID,
                       const nsAString& aDisplayName,
                       const nsAString& aLocalizationKey,
                       const bool aRemoteReadable,
                       const bool aRemoteWritable,
                       const bool aUserViewable,
                       const bool aUserEditable);
  virtual ~sbRatingPropertyInfo() {}

  NS_IMETHOD Format(const nsAString& aValue, nsAString& _retval);
  NS_IMETHOD Validate(const nsAString& aValue, bool* _retval);

  nsresult Init();

  nsresult InitializeOperators();
private:
  bool mSuppressSelect;
};

#endif /* __SBRATINGPROPERTYINFO_H__ */

