#
# This cmake module sets the project version and partial version
# variables by analysing the git tag and commit history. It expects git
# tags defined with semantic versioning 2.0.0 (http://semver.org/).
#
# The module expects the PROJECT_NAME variable to be set, and recognizes
# the GIT_FOUND, GIT_EXECUTABLE and VERSION_UPDATE_FROM_GIT variables.
# If Git is found and VERSION_UPDATE_FROM_GIT is set to boolean TRUE,
# the project version will be updated using information fetched from the
# most recent git tag and commit. Otherwise, the module will try to read
# a VERSION file containing the full and partial versions. The module
# will update this file each time the project version is updated.
#
# Once done, this module will define the following variables:
#
# ${PROJECT_NAME}_VERSION_STRING - Version string without metadata
# such as "v2.0.0" or "v1.2.41-beta.1". This should correspond to the
# most recent git tag.
# ${PROJECT_NAME}_VERSION_STRING_FULL - Version string with metadata
# such as "v2.0.0+3.a23fbc" or "v1.3.1-alpha.2+4.9c4fd1"
# ${PROJECT_NAME}_VERSION - Same as ${PROJECT_NAME}_VERSION_STRING,
# without the preceding 'v', e.g. "2.0.0" or "1.2.41-beta.1"
# ${PROJECT_NAME}_VERSION_MAJOR - Major version integer (e.g. 2 in v2.3.1-RC.2+21.ef12c8)
# ${PROJECT_NAME}_VERSION_MINOR - Minor version integer (e.g. 3 in v2.3.1-RC.2+21.ef12c8)
# ${PROJECT_NAME}_VERSION_PATCH - Patch version integer (e.g. 1 in v2.3.1-RC.2+21.ef12c8)
# ${PROJECT_NAME}_VERSION_AHEAD - How many commits ahead of last tag (e.g. 21 in v2.3.1-RC.2+21.ef12c8)
# ${PROJECT_NAME}_VERSION_GIT_SHA - The git sha1 of the most recent commit (e.g. the "ef12c8" in v2.3.1-RC.2+21.ef12c8)
#
# This module is public domain, use it as it fits you best.
#
# Author: Nuno Fachada
# Modified by: José Fonseca

set(${PROJECT_NAME}_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")

set(${PROJECT_NAME}_GIT_DESCRIBE_RESULT 1)

if (GIT_FOUND)

	# Get last tag from git
	execute_process(COMMAND ${GIT_EXECUTABLE} describe --abbrev=0 --tags
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE ${PROJECT_NAME}_GIT_DESCRIBE_RESULT
		OUTPUT_VARIABLE ${PROJECT_NAME}_VERSION_STRING
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE)

endif()

if (${PROJECT_NAME}_GIT_DESCRIBE_RESULT EQUAL 0)

	#How many commits since last tag
	execute_process(COMMAND ${GIT_EXECUTABLE} rev-list main ${${PROJECT_NAME}_VERSION_STRING}..HEAD --count
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE ${PROJECT_NAME}_VERSION_AHEAD
		OUTPUT_STRIP_TRAILING_WHITESPACE)

	# Get current commit SHA from git
	execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE ${PROJECT_NAME}_VERSION_GIT_SHA
		OUTPUT_STRIP_TRAILING_WHITESPACE)

	# Get partial versions into a list
	string(REGEX MATCHALL "-.*$|[0-9]+" ${PROJECT_NAME}_PARTIAL_VERSION_LIST ${${PROJECT_NAME}_VERSION_STRING})

	# Set the version numbers
	list(GET ${PROJECT_NAME}_PARTIAL_VERSION_LIST
		0 ${PROJECT_NAME}_VERSION_MAJOR)
	list(GET ${PROJECT_NAME}_PARTIAL_VERSION_LIST
		1 ${PROJECT_NAME}_VERSION_MINOR)
	list(GET ${PROJECT_NAME}_PARTIAL_VERSION_LIST
		2 ${PROJECT_NAME}_VERSION_PATCH)

	# Unset the list
	unset(${PROJECT_NAME}_PARTIAL_VERSION_LIST)

	# Set full project version string
	set(${PROJECT_NAME}_VERSION_STRING_FULL
		${${PROJECT_NAME}_VERSION_STRING}+${${PROJECT_NAME}_VERSION_AHEAD}.${${PROJECT_NAME}_VERSION_GIT_SHA})

elseif (EXISTS ${${PROJECT_NAME}_VERSION_FILE})

	# Git is unavailable or has no tags yet; fall back to the VERSION file
	file(STRINGS ${${PROJECT_NAME}_VERSION_FILE} ${PROJECT_NAME}_VERSION_STRING LIMIT_COUNT 1)

	string(REGEX MATCHALL "[0-9]+" ${PROJECT_NAME}_PARTIAL_VERSION_LIST ${${PROJECT_NAME}_VERSION_STRING})

	list(GET ${PROJECT_NAME}_PARTIAL_VERSION_LIST
		0 ${PROJECT_NAME}_VERSION_MAJOR)
	list(GET ${PROJECT_NAME}_PARTIAL_VERSION_LIST
		1 ${PROJECT_NAME}_VERSION_MINOR)
	list(GET ${PROJECT_NAME}_PARTIAL_VERSION_LIST
		2 ${PROJECT_NAME}_VERSION_PATCH)

	unset(${PROJECT_NAME}_PARTIAL_VERSION_LIST)

	set(${PROJECT_NAME}_VERSION_STRING_FULL ${${PROJECT_NAME}_VERSION_STRING})

else()

	# No git tags and no VERSION file: default to an unversioned build
	set(${PROJECT_NAME}_VERSION_MAJOR 0)
	set(${PROJECT_NAME}_VERSION_MINOR 0)
	set(${PROJECT_NAME}_VERSION_PATCH 0)
	set(${PROJECT_NAME}_VERSION_STRING "v0.0.0")
	set(${PROJECT_NAME}_VERSION_STRING_FULL "v0.0.0")

endif()

# Set project version (without the preceding 'v')
set(${PROJECT_NAME}_VERSION ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}.${${PROJECT_NAME}_VERSION_PATCH})
