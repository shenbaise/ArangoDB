/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, $, window*/
(function () {
    "use strict";
    window.StatisticBarView = Backbone.View.extend({
        el: '#statisticBar',

        events: {
            "change #arangoCollectionSelect": "navigateBySelect",
            "click .tab": "navigateByTab"
        },

        template: templateEngine.createTemplate("statisticBarView.ejs"),

        render: function () {
            $(this.el).html(this.template.render({
              isSystem: window.currentDB.get("isSystem")
            }));
            
            $('img.svg').each(function(){
                        var $img = $(this);
                        var imgID = $img.attr('id');
                        var imgClass = $img.attr('class');
                        var imgURL = $img.attr('src');

                        $.get(imgURL, function(data) {
                            // Get the SVG tag, ignore the rest
                            var $svg = $(data).find('svg');

                            // Add replaced image's ID to the new SVG
                            if(typeof imgID !== 'undefined') {
                                $svg = $svg.attr('id', imgID);
                            }
                            // Add replaced image's classes to the new SVG
                            if(typeof imgClass !== 'undefined') {
                                $svg = $svg.attr('class', imgClass+' replaced-svg');
                            }

                            // Remove any invalid XML tags as per http://validator.w3.org
                            $svg = $svg.removeAttr('xmlns:a');

                            // Replace image with new SVG
                            $img.replaceWith($svg);

                        }, 'xml');

                    });            
            return this;
        },

        navigateBySelect: function () {
            var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
            window.App.navigate(navigateTo, {trigger: true});
        },

        navigateByTab: function (e) {
            var tab = e.target || e.srcElement;
            var navigateTo = tab.id;
            if (navigateTo === "links") {
                $("#link_dropdown").slideToggle(200);
                e.preventDefault();
                return;
            }
            if (navigateTo === "tools") {
                $("#tools_dropdown").slideToggle(200);
                e.preventDefault();
                return;
            }
            window.App.navigate(navigateTo, {trigger: true});
            e.preventDefault();
        },
        handleSelectNavigation: function () {
            $("#arangoCollectionSelect").change(function () {
                var navigateTo = $(this).find("option:selected").val();
                window.App.navigate(navigateTo, {trigger: true});
            });
        },


        selectMenuItem: function (menuItem) {
            $('.navlist li').removeClass('active');
            if (menuItem) {
                $('.' + menuItem).addClass('active');
            }
        }

    });
}());
