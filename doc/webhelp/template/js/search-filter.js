/**
 * Object that implements the methods required by WebHelp to run a search filter.
 */
function CustomSearchFilter() {

    /**
     * Method required to run the search filter in webhelp. It is called when the users 
     * executes the query in the search page. 
     * 
     * @param {WebHelpAPI.SearchResult} searchResult The search result for the executed query.
     *
     * @return A list of WebHelpAPI.SearchResult objects
     */
    this.filterResults = function (searchResult) {
        // implement filter
        return filteredResults;
    }
}

// Set the Search Filter to WebHelp
WebHelpAPI.setCustomSearchFilter(new CustomSearchFilter());