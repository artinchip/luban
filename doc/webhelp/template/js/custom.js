var executed = false;
$( document ).ready(function() {
    $("#searchForm").on("submit", (e) => {
        // WebHelps triggers the submit event handler multiple times.
        if(!executed) { // We make sure that we execute it only one time.
            e.stopPropagation(); 
            var userQuery = $('#textToSearch').val();
            if (userQuery.trim() === '') {
                e.preventDefault();
                return false;
            }
            
            if (!/^[a-zA-Z]+$/.test(userQuery)) {
                userQuery = userQuery.replace(/[\u4e00-\u9fa5]{2}/g, '$& '); // if the input isn't english characters, split every given Chinese input (string) by separating every two Chinese chars by spaces. 
              }
            // userQuery = userQuery.replace(/[\u4e00-\u9fa5]{2}/g, '$& '); // split every given Chinese input by separating every two Chinese characters by spaces.
            //userQuery = userQuery.replace(/[\u4e00-\u9fa5]/g, '$& ');  // split every given Chinese input by separating every Chinese character by spaces.
            $('#textToSearch').val(userQuery);
            executed = true;
        }
    });
});