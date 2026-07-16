# Various checks related to Boost

set(BOOST_USE_STATIC_LIBS OFF)
set(BOOST_USE_MULTITHREADED OFF)
set(BOOST_USE_STATIC_RUNTIME OFF)
if (HAVE_LIBCPP)
    # we need a more recent boost for libc++
    set(BOOST_MINVERSION 1.61.0)
else ()
    set(BOOST_MINVERSION 1.57.0)
endif ()

set(_BOOST_HINT "${CMAKE_CURRENT_LIST_DIR}/../include")
if (EXISTS "${_BOOST_HINT}/boost/version.hpp")
    set(Boost_FOUND TRUE)
    set(Boost_INCLUDE_DIRS "${_BOOST_HINT}")
    message(STATUS "Using vendored Boost headers at ${Boost_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR
      "Boost headers not found at ${_BOOST_HINT}/boost/version.hpp")
endif()

message(STATUS "Boost version: ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")

# Boost 1.62 has a bug that we've patched around, check if it is required
if (Boost_VERSION EQUAL 106200)
    set (CMAKE_REQUIRED_INCLUDES ${BOOST_INCLUDEDIR} "${PROJECT_SOURCE_DIR}/include")
    set (BOOST_REV_TEST "
#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/concept/assert.hpp>

int main(int,char*[])
{
  using namespace boost;
  // Check const reverse_graph
  {
    typedef adjacency_list< vecS, vecS, bidirectionalS,
      property<vertex_color_t, int>,
      property<edge_weight_t, int>,
      property<graph_name_t, std::string>
    > AdjList;
    typedef reverse_graph<AdjList> Graph;
    BOOST_CONCEPT_ASSERT(( BidirectionalGraphConcept<Graph> ));
  }
  return 0;
}
")

    CHECK_CXX_SOURCE_COMPILES("${BOOST_REV_TEST}" BOOST_REVGRAPH_OK)

    if (NOT BOOST_REVGRAPH_OK)
        message(STATUS "trying patched")
        CHECK_CXX_SOURCE_COMPILES("
#include <boost-patched/graph/reverse_graph.hpp>
${BOOST_REV_TEST}" BOOST_REVGRAPH_PATCH)
    endif()

    if (NOT BOOST_REVGRAPH_OK AND NOT BOOST_REVGRAPH_PATCH)
        message(FATAL_ERROR "Something is wrong with this copy of boost::reverse_graph")
    endif()

    unset (CMAKE_REQUIRED_INCLUDES)
else ()
    unset(BOOST_REVGRAPH_OK CACHE)
    unset(BOOST_REVGRAPH_PATCH CACHE)
endif () # Boost 1.62.0
