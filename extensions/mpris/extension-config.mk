#
#=BEGIN NIGHTINGALE GPL
#
# This file is part of the Nightingale web player.
#
# http://getnightingale.com
#
# This file may be licensed under the terms of of the
# GNU General Public License Version 2 (the "GPL").
#
# Software distributed under the License is distributed
# on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either
# express or implied. See the GPL for the specific language
# governing rights and limitations.
#
# You should have received a copy of the GPL along with this
# program. If not, go to http://www.gnu.org/licenses/gpl.html
# or write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
#=END NIGHTINGALE GPL
#

EXTENSION_NAME = mpris
EXTENSION_UUID = $(EXTENSION_NAME)@getnightingale.com

EXTENSION_VER = 1.1.0.0
EXTENSION_MIN_VER = $(SB_BINARY_EXTENSION_MIN_VER)
EXTENSION_MAX_VER = $(SB_BINARY_EXTENSION_MAX_VER)

# This is needed to build since targetPlatform can't be set install.rdf
EXTENSION_SUPPORTED_PLATFORMS = linux
