#!/bin/bash

# Yggdrasil
# Copyright (C) 2026 Zeeshan Qazi
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

set -e

# Change to the directory of the script
cd "$(dirname "$0")"

echo "Building Driver License Example..."
# Ensure g++-16 is available. P2996 features require g++ with -freflection
g++-16 -std=c++26 -freflection -I../include driver_license.cpp -o driver_license

echo ""
echo "Running Driver License Example..."
echo "================================="
./driver_license
